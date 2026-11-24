#include "database/MySqlUserRepository.h"

#include <cstdio>

#include "utils/Logger.h"

namespace uniconnect {
namespace database {

namespace {
// One query returns the user plus every profile id; the department comes from the faculty row, or from
// the student's section -> program -> department.
const char* kSelectUser =
    "SELECT u.user_id, u.name, u.email, u.password_hash, u.role, "
    "       COALESCE(f.department_id, p.department_id) AS department_id, "
    "       s.student_id, f.faculty_id, s.section_id "
    "FROM users u "
    "LEFT JOIN students s ON s.user_id = u.user_id "
    "LEFT JOIN sections sec ON sec.section_id = s.section_id "
    "LEFT JOIN programs p ON p.program_id = sec.program_id "
    "LEFT JOIN faculty f ON f.user_id = u.user_id ";

models::User toUser(const DbRow& r) {
    models::User u;
    u.id = rowInt(r, "user_id");
    u.name = rowStr(r, "name");
    u.email = rowStr(r, "email");
    u.password_hash = rowStr(r, "password_hash");
    u.role = models::stringToUserRole(rowStr(r, "role"));
    u.department_id = rowInt(r, "department_id");
    u.student_id = rowInt(r, "student_id");
    u.faculty_id = rowInt(r, "faculty_id");
    u.section_id = rowInt(r, "section_id");
    return u;
}

std::string padded(const char* prefix, unsigned long long id) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%s%06llu", prefix, id);
    return buf;
}

CreateUserResult failure(const std::string& msg, int status = 400) {
    CreateUserResult r;
    r.error = msg;
    r.status = status;
    return r;
}
} // namespace

std::optional<models::User> MySqlUserRepository::findOne(const std::string& where, const DbValue& value) {
    const auto r = db_.execute(std::string(kSelectUser) + "WHERE " + where + " LIMIT 1", {value});
    if (!r.ok || r.rows.empty()) return std::nullopt;
    return toUser(r.rows.front());
}

std::optional<models::User> MySqlUserRepository::findByEmail(const std::string& email) {
    return findOne("u.email = ?", email);
}

std::optional<models::User> MySqlUserRepository::findById(int id) {
    return findOne("u.user_id = ?", std::to_string(id));
}

bool MySqlUserRepository::departmentExists(int department_id) {
    const auto r = db_.execute("SELECT 1 AS present FROM departments WHERE department_id = ? LIMIT 1",
                               {std::to_string(department_id)});
    return r.ok && !r.rows.empty();
}

bool MySqlUserRepository::healthy() {
    return db_.execute("SELECT 1 AS ok").ok;
}

CreateUserResult MySqlUserRepository::createUser(const NewUser& n) {
    int new_id = 0;
    {
        MySqlDatabase::Transaction tx(db_);   // rolls back on any early return
        if (!tx.ok()) return failure("The database is not available. Please try again later.", 503);

        const auto ins = tx.execute("INSERT INTO users (name, email, password_hash, role) VALUES (?, ?, ?, ?)",
                                    {n.name, n.email, n.password_hash, models::userRoleToString(n.role)});
        if (!ins.ok) {
            if (ins.error_code == 1062) return failure("An account with this email already exists.", 409);
            return failure("Could not create the account.", 500);
        }
        new_id = static_cast<int>(ins.insert_id);
        const std::string uid = std::to_string(new_id);

        if (n.role == models::UserRole::STUDENT) {
            // The section must belong to the chosen department; default = the department's first section.
            DbResult sec;
            if (n.section_id) {
                sec = tx.execute("SELECT sec.section_id FROM sections sec JOIN programs p ON p.program_id = sec.program_id "
                                 "WHERE sec.section_id = ? AND p.department_id = ?",
                                 {std::to_string(*n.section_id), std::to_string(n.department_id)});
                if (sec.ok && sec.rows.empty()) return failure("That section does not belong to the selected department.");
            } else {
                sec = tx.execute("SELECT sec.section_id FROM sections sec JOIN programs p ON p.program_id = sec.program_id "
                                 "WHERE p.department_id = ? ORDER BY sec.section_id LIMIT 1",
                                 {std::to_string(n.department_id)});
                if (sec.ok && sec.rows.empty()) return failure("No section is available for this department.");
            }
            if (!sec.ok) return failure("Could not create the account.", 500);
            const auto st = tx.execute("INSERT INTO students (user_id, roll_no, section_id) VALUES (?, ?, ?)",
                                       {uid, padded("REG", static_cast<unsigned long long>(new_id)), rowStr(sec.rows[0], "section_id")});
            if (!st.ok) return failure("Could not create the student profile.", 500);
        } else if (n.role == models::UserRole::FACULTY) {
            const auto f = tx.execute("INSERT INTO faculty (user_id, employee_id, department_id) VALUES (?, ?, ?)",
                                      {uid, padded("EMP", static_cast<unsigned long long>(new_id)), std::to_string(n.department_id)});
            if (!f.ok) return failure("Could not create the faculty profile.", 500);
        } else {
            const auto a = tx.execute("INSERT INTO admins (user_id) VALUES (?)", {uid});
            if (!a.ok) return failure("Could not create the admin profile.", 500);
        }
        if (!tx.commit()) return failure("Could not save the account.", 500);
    }

    CreateUserResult r;
    if (auto u = findById(new_id)) { r.ok = true; r.user = *u; }
    else { r.error = "Account was created but could not be loaded."; r.status = 500; }
    return r;
}

} // namespace database
} // namespace uniconnect
