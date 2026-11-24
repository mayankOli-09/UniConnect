// mysql.h on Windows needs the socket types first.
#include <winsock2.h>
#include <mysql.h>

#include "database/MySqlDatabase.h"

#include <cstring>
#include <memory>
#include <mutex>

#include "utils/Env.h"
#include "utils/Logger.h"

#if defined(_MSC_VER)
#pragma comment(lib, "libmysql.lib")
#endif

namespace uniconnect {
namespace database {

namespace {
// The MySQL client library needs per-thread setup in threads that did not create the connection.
struct ThreadGuard {
    ThreadGuard() { mysql_thread_init(); }
    ~ThreadGuard() { mysql_thread_end(); }
};
void ensureLibrary() {
    static std::once_flag once;
    std::call_once(once, [] { mysql_library_init(0, nullptr, nullptr); });
}
} // namespace

DbConfig DbConfig::fromEnvironment() {
    DbConfig c;
    c.host = utils::getEnvVar("MYSQL_HOST", c.host);
    const std::string port = utils::getEnvVar("MYSQL_PORT");
    if (!port.empty()) { try { c.port = static_cast<unsigned>(std::stoul(port)); } catch (...) {} }
    c.user = utils::getEnvVar("MYSQL_USER", c.user);
    c.password = utils::getEnvVar("MYSQL_PASSWORD", c.password);
    c.database = utils::getEnvVar("MYSQL_DB", c.database);
    return c;
}

MySqlDatabase::MySqlDatabase(DbConfig config) : config_(std::move(config)) { ensureLibrary(); }

MySqlDatabase::~MySqlDatabase() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    closeLocked();
}

void MySqlDatabase::closeLocked() {
    if (conn_) { mysql_close(conn_); conn_ = nullptr; }
}

bool MySqlDatabase::connect(std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return connectLocked(error);
}

bool MySqlDatabase::connectLocked(std::string& error) {
    static thread_local ThreadGuard guard;
    closeLocked();
    conn_ = mysql_init(nullptr);
    if (!conn_) { error = "mysql_init failed (out of memory)"; return false; }
    unsigned timeout = 5;
    mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    timeout = 30;
    mysql_options(conn_, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(conn_, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
    mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    mysql_options(conn_, MYSQL_INIT_COMMAND, "SET time_zone = '+00:00'");
    if (!mysql_real_connect(conn_, config_.host.c_str(), config_.user.c_str(), config_.password.c_str(),
                            config_.database.c_str(), config_.port, nullptr, 0)) {
        error = std::string(mysql_error(conn_)) + " (MySQL error " + std::to_string(mysql_errno(conn_)) + ")";
        closeLocked();
        return false;
    }
    return true;
}

bool MySqlDatabase::ensureAliveLocked(std::string& error) {
    if (conn_ && (in_transaction_ || mysql_ping(conn_) == 0)) return true;   // cannot reconnect mid-transaction
    if (in_transaction_) { error = "Database connection lost during a transaction"; return false; }
    utils::Logger::getInstance().warn("Database", "Connection lost, reconnecting...");
    return connectLocked(error);
}

bool MySqlDatabase::simple(const char* sql) {
    return conn_ && mysql_real_query(conn_, sql, static_cast<unsigned long>(std::strlen(sql))) == 0;
}

DbResult MySqlDatabase::execute(const std::string& sql, const std::vector<DbValue>& params) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    static thread_local ThreadGuard guard;
    DbResult r;
    if (!ensureAliveLocked(r.error)) return r;
    return run(sql, params);
}

DbResult MySqlDatabase::run(const std::string& sql, const std::vector<DbValue>& params) {
    DbResult res;
    MYSQL_STMT* st = mysql_stmt_init(conn_);
    if (!st) { res.error = "mysql_stmt_init failed (out of memory)"; return res; }

    MYSQL_RES* meta = nullptr;
    auto fail = [&]() -> DbResult {
        res.ok = false;
        res.error = mysql_stmt_error(st);
        res.error_code = mysql_stmt_errno(st);
        if (meta) mysql_free_result(meta);
        mysql_stmt_close(st);
        utils::Logger::getInstance().warn("Database", "SQL error", res.error_code, ":", res.error);
        return res;
    };

    if (mysql_stmt_prepare(st, sql.c_str(), static_cast<unsigned long>(sql.size()))) return fail();
    if (mysql_stmt_param_count(st) != params.size()) {
        res.error = "Wrong number of SQL parameters";
        mysql_stmt_close(st);
        return res;
    }

    // ---- bind parameters (all as text; MySQL converts to the column type)
    std::vector<MYSQL_BIND> in(params.size());
    std::vector<unsigned long> lens(params.size(), 0);
    std::unique_ptr<bool[]> nulls(new bool[params.size() + 1]());
    std::memset(in.data(), 0, in.size() * sizeof(MYSQL_BIND));
    for (size_t i = 0; i < params.size(); ++i) {
        if (!params[i]) {
            nulls[i] = true;
            in[i].buffer_type = MYSQL_TYPE_NULL;
            in[i].is_null = &nulls[i];
        } else {
            lens[i] = static_cast<unsigned long>(params[i]->size());
            in[i].buffer_type = MYSQL_TYPE_STRING;
            in[i].buffer = const_cast<char*>(params[i]->data());
            in[i].buffer_length = lens[i];
            in[i].length = &lens[i];
        }
    }
    if (!params.empty() && mysql_stmt_bind_param(st, in.data())) return fail();
    if (mysql_stmt_execute(st)) return fail();

    // ---- read the result set, if the statement produced one
    meta = mysql_stmt_result_metadata(st);
    if (meta) {
        const unsigned n = mysql_num_fields(meta);
        MYSQL_FIELD* fields = mysql_fetch_fields(meta);
        std::vector<std::string> names;
        for (unsigned i = 0; i < n; ++i) names.emplace_back(fields[i].name);

        std::vector<MYSQL_BIND> out(n);
        std::memset(out.data(), 0, out.size() * sizeof(MYSQL_BIND));
        std::vector<unsigned long> olen(n, 0);
        std::unique_ptr<bool[]> onull(new bool[n]());
        std::unique_ptr<bool[]> oerr(new bool[n]());
        for (unsigned i = 0; i < n; ++i) {
            out[i].buffer_type = MYSQL_TYPE_STRING;   // zero-length buffer: we learn the size, then fetch the column
            out[i].buffer = nullptr;
            out[i].buffer_length = 0;
            out[i].length = &olen[i];
            out[i].is_null = &onull[i];
            out[i].error = &oerr[i];
        }
        if (mysql_stmt_bind_result(st, out.data())) return fail();
        if (mysql_stmt_store_result(st)) return fail();

        while (true) {
            const int rc = mysql_stmt_fetch(st);
            if (rc == MYSQL_NO_DATA) break;
            if (rc == 1) return fail();                // 0 and MYSQL_DATA_TRUNCATED both mean "row available"
            DbRow row;
            for (unsigned i = 0; i < n; ++i) {
                if (onull[i]) { row[names[i]] = std::nullopt; continue; }
                std::string v(olen[i], '\0');
                if (olen[i] > 0) {
                    MYSQL_BIND b;
                    std::memset(&b, 0, sizeof(b));
                    unsigned long got = 0;
                    b.buffer_type = MYSQL_TYPE_STRING;
                    b.buffer = &v[0];
                    b.buffer_length = olen[i];
                    b.length = &got;
                    if (mysql_stmt_fetch_column(st, &b, i, 0)) return fail();
                }
                row[names[i]] = std::move(v);
            }
            res.rows.push_back(std::move(row));
        }
        mysql_free_result(meta);
        meta = nullptr;
    } else {
        res.affected_rows = mysql_stmt_affected_rows(st);
    }
    res.insert_id = mysql_stmt_insert_id(st);
    mysql_stmt_close(st);
    res.ok = true;
    return res;
}

// ------------------------------------------------------------------ Transaction
MySqlDatabase::Transaction::Transaction(MySqlDatabase& db) : db_(db), lock_(db.mutex_) {
    std::string err;
    if (!db_.ensureAliveLocked(err)) return;
    started_ = db_.simple("START TRANSACTION");
    db_.in_transaction_ = started_;
}

MySqlDatabase::Transaction::~Transaction() {
    if (started_ && !finished_) db_.simple("ROLLBACK");
    if (started_) db_.in_transaction_ = false;
}

DbResult MySqlDatabase::Transaction::execute(const std::string& sql, const std::vector<DbValue>& params) {
    DbResult r;
    if (!started_ || finished_) { r.error = "Transaction is not active"; return r; }
    return db_.run(sql, params);
}

bool MySqlDatabase::Transaction::commit() {
    if (!started_ || finished_) return false;
    finished_ = true;
    const bool ok = db_.simple("COMMIT");
    if (!ok) db_.simple("ROLLBACK");
    db_.in_transaction_ = false;
    return ok;
}

} // namespace database
} // namespace uniconnect
