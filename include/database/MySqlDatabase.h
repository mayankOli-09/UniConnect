#pragma once
// Thin, thread-safe wrapper over the MySQL C API. Every value travels as a bound parameter of a
// prepared statement (never pasted into the SQL text), so user input cannot change a query.
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct MYSQL;   // from <mysql.h>

namespace uniconnect {
namespace database {

struct DbConfig {
    std::string host = "127.0.0.1";
    unsigned port = 3306;
    std::string user = "uniconnect";
    std::string password;
    std::string database = "uniconnect";

    // MYSQL_HOST, MYSQL_PORT, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DB
    static DbConfig fromEnvironment();
};

using DbValue = std::optional<std::string>;        // std::nullopt = SQL NULL
using DbRow = std::map<std::string, DbValue>;      // column name (or alias) -> value

inline std::string rowStr(const DbRow& r, const std::string& key, const std::string& def = "") {
    auto it = r.find(key);
    return (it == r.end() || !it->second) ? def : *it->second;
}
inline int rowInt(const DbRow& r, const std::string& key, int def = 0) {
    auto it = r.find(key);
    if (it == r.end() || !it->second || it->second->empty()) return def;
    try { return std::stoi(*it->second); } catch (...) { return def; }
}

struct DbResult {
    bool ok = false;
    std::string error;
    unsigned error_code = 0;               // MySQL error number, e.g. 1062 = duplicate key
    std::vector<DbRow> rows;               // SELECT results
    unsigned long long insert_id = 0;      // AUTO_INCREMENT id of an INSERT
    unsigned long long affected_rows = 0;  // rows changed by INSERT/UPDATE/DELETE
};

class MySqlDatabase {
public:
    explicit MySqlDatabase(DbConfig config);
    ~MySqlDatabase();
    MySqlDatabase(const MySqlDatabase&) = delete;
    MySqlDatabase& operator=(const MySqlDatabase&) = delete;

    bool connect(std::string& error);
    // `sql` uses ? placeholders; params are bound in order.
    DbResult execute(const std::string& sql, const std::vector<DbValue>& params = {});

    // All-or-nothing group of statements. Rolls back automatically unless commit() succeeded.
    class Transaction {
    public:
        explicit Transaction(MySqlDatabase& db);
        ~Transaction();
        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;
        bool ok() const { return started_; }
        DbResult execute(const std::string& sql, const std::vector<DbValue>& params = {});
        bool commit();
    private:
        MySqlDatabase& db_;
        std::unique_lock<std::recursive_mutex> lock_;
        bool started_ = false;
        bool finished_ = false;
    };

private:
    bool connectLocked(std::string& error);
    bool ensureAliveLocked(std::string& error);
    DbResult run(const std::string& sql, const std::vector<DbValue>& params);
    bool simple(const char* sql);
    void closeLocked();

    DbConfig config_;
    MYSQL* conn_ = nullptr;
    bool in_transaction_ = false;
    std::recursive_mutex mutex_;
};

} // namespace database
} // namespace uniconnect
