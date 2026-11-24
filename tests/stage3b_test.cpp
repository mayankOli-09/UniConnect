// Stage 3b check: login/register against the real MySQL database (needs MySQL running with the uniconnect schema).
#include <chrono>
#include <iostream>
#include <string>

#include "database/MySqlDatabase.h"
#include "database/MySqlUserRepository.h"
#include "services/AuthService.h"
#include "utils/Crypto.h"
#include "utils/Env.h"

using namespace uniconnect;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

static void cleanup(database::MySqlDatabase& db) {
    const std::string like = "stage3b.%@test.local";
    db.execute("DELETE FROM students WHERE user_id IN (SELECT user_id FROM users WHERE email LIKE ?)", {like});
    db.execute("DELETE FROM faculty  WHERE user_id IN (SELECT user_id FROM users WHERE email LIKE ?)", {like});
    db.execute("DELETE FROM admins   WHERE user_id IN (SELECT user_id FROM users WHERE email LIKE ?)", {like});
    db.execute("DELETE FROM users WHERE email LIKE ?", {like});
    db.execute("DELETE FROM departments WHERE department_name = ?", {std::string("Stage3b Temp Dept")});
}

int main() {
    database::DbConfig cfg = database::DbConfig::fromEnvironment();
    if (cfg.password.empty()) cfg.password = "UniConnect#Dev2026";   // documented local development password

    std::cout << "1. connect to MySQL as '" << cfg.user << "' on " << cfg.host << ":" << cfg.port << "\n";
    database::MySqlDatabase db(cfg);
    std::string err;
    if (!db.connect(err)) {
        std::cout << "   CANNOT CONNECT: " << err << "\n"
                  << "   Is MySQL running? Are MYSQL_USER / MYSQL_PASSWORD / MYSQL_DB correct?\n";
        return 1;
    }
    cleanup(db);   // in case an earlier run was interrupted

    std::cout << "2. read the seeded users\n";
    database::MySqlUserRepository repo(db);
    auto rahul = repo.findByEmail("rahul@uniconnect.com");
    CHECK(rahul && rahul->role == models::UserRole::STUDENT && rahul->student_id > 0 && rahul->section_id > 0 && rahul->department_id > 0);
    auto rajesh = repo.findByEmail("rajesh@uniconnect.com");
    CHECK(rajesh && rajesh->role == models::UserRole::FACULTY && rajesh->faculty_id > 0 && rajesh->department_id > 0 && rajesh->student_id == 0);
    auto admin = repo.findByEmail("admin@uniconnect.com");
    CHECK(admin && admin->role == models::UserRole::ADMIN && admin->student_id == 0 && admin->faculty_id == 0);
    CHECK(!repo.findByEmail("nobody@uniconnect.com"));
    CHECK(repo.findById(1) && repo.findById(1)->email == "rahul@uniconnect.com");
    CHECK(repo.departmentExists(1) && repo.departmentExists(3) && !repo.departmentExists(999));
    if (rahul) std::cout << "   Rahul: student_id=" << rahul->student_id << " section_id=" << rahul->section_id
                         << " department_id=" << rahul->department_id << "\n";

    std::cout << "3. log in with the seeded accounts (Password@123)\n";
    services::AuthConfig ac;
    ac.jwt_secret = "stage3b-test-secret-0123456789";
    ac.allow_privileged_signup = true;
    services::AuthService auth(repo, ac);
    auto l1 = auth.login("rahul@uniconnect.com", "Password@123");
    CHECK(l1.success && l1.user.student_id > 0 && auth.verifyToken(l1.token));
    CHECK(auth.login("rajesh@uniconnect.com", "Password@123").success);
    CHECK(auth.login("admin@uniconnect.com", "Password@123").user.role == models::UserRole::ADMIN);
    CHECK(!auth.login("rahul@uniconnect.com", "wrong-password").success);
    CHECK(!auth.login("nobody@uniconnect.com", "Password@123").success);

    std::cout << "4. register new users (students, faculty, admin)\n";
    const std::string tag = utils::crypto::toHex(utils::crypto::randomBytes(4));
    const std::string e1 = "stage3b." + tag + ".s1@test.local";
    auto s1 = auth.registerUser("Test Student", e1, "Password@123", models::UserRole::STUDENT, "2");
    CHECK(s1.success && s1.user.student_id > 0 && s1.user.section_id > 0 && s1.user.department_id == 2);
    auto dbCheck = db.execute("SELECT roll_no FROM students WHERE user_id = ?", {std::to_string(s1.user.id)});
    CHECK(dbCheck.ok && dbCheck.rows.size() == 1 && database::rowStr(dbCheck.rows[0], "roll_no").rfind("REG", 0) == 0);
    CHECK(auth.login(e1, "Password@123").success);                                           // can log in right away
    CHECK(!auth.registerUser("Dup", "STAGE3B." + tag + ".S1@TEST.LOCAL", "Password@123", models::UserRole::STUDENT, "2").success);
    auto fac = auth.registerUser("Test Prof", "stage3b." + tag + ".f1@test.local", "Password@123", models::UserRole::FACULTY, "1");
    CHECK(fac.success && fac.user.faculty_id > 0 && fac.user.department_id == 1);
    auto adm = auth.registerUser("Test Admin", "stage3b." + tag + ".a1@test.local", "Password@123", models::UserRole::ADMIN, "1");
    CHECK(adm.success && adm.user.role == models::UserRole::ADMIN);
    auto cnt = db.execute("SELECT COUNT(*) AS n FROM admins WHERE user_id = ?", {std::to_string(adm.user.id)});
    CHECK(cnt.ok && database::rowInt(cnt.rows[0], "n") == 1);

    std::cout << "5. explicit section must belong to the department\n";
    CHECK(!auth.registerUser("Wrong Sec", "stage3b." + tag + ".w1@test.local", "Password@123", models::UserRole::STUDENT, "1", "3").success);
    CHECK(!repo.findByEmail("stage3b." + tag + ".w1@test.local"));                           // nothing left behind

    std::cout << "6. failed registration is rolled back completely\n";
    db.execute("INSERT INTO departments (department_name) VALUES (?)", {std::string("Stage3b Temp Dept")});
    auto tmp = db.execute("SELECT department_id FROM departments WHERE department_name = ?", {std::string("Stage3b Temp Dept")});
    const std::string tmpDept = tmp.ok && !tmp.rows.empty() ? database::rowStr(tmp.rows[0], "department_id") : "";
    CHECK(!tmpDept.empty());
    auto nosec = auth.registerUser("No Section", "stage3b." + tag + ".n1@test.local", "Password@123", models::UserRole::STUDENT, tmpDept);
    CHECK(!nosec.success && nosec.error_message.find("No section") != std::string::npos);
    CHECK(!repo.findByEmail("stage3b." + tag + ".n1@test.local"));                           // users row was rolled back

    std::cout << "7. SQL-injection attempt and unicode are stored as plain text\n";
    const std::string evil = "Robert'); DROP TABLE users;-- \\";
    auto inj = auth.registerUser(evil, "stage3b." + tag + ".i1@test.local", "Password@123", models::UserRole::STUDENT, "1");
    CHECK(inj.success && inj.user.name == evil);
    auto again = repo.findByEmail("stage3b." + tag + ".i1@test.local");
    CHECK(again && again->name == evil);
    CHECK(repo.findByEmail("rahul@uniconnect.com").has_value());                              // users table still exists
    CHECK(!auth.login("' OR '1'='1", "x").success && !auth.login("rahul@uniconnect.com' --", "x").success);
    const std::string uni = "Zo\xC3\xAB \xE7\x8E\x8B \xF0\x9F\x8E\x93";                       // Zoë 王 🎓
    auto un = auth.registerUser(uni, "stage3b." + tag + ".u1@test.local", "Password@123", models::UserRole::STUDENT, "1");
    CHECK(un.success && un.user.name == uni);
    auto un2 = repo.findByEmail("stage3b." + tag + ".u1@test.local");
    CHECK(un2 && un2->name == uni);

    std::cout << "8. recovers after the server drops the connection\n";
    auto id = db.execute("SELECT CONNECTION_ID() AS id");
    CHECK(id.ok && !id.rows.empty());
    database::MySqlDatabase killer(cfg);
    CHECK(killer.connect(err));
    killer.execute("KILL " + database::rowStr(id.rows[0], "id"));   // an id we just read from MySQL, not user input
    auto after = db.execute("SELECT 1 AS one");
    CHECK(after.ok && !after.rows.empty());
    CHECK(auth.login("rahul@uniconnect.com", "Password@123").success);

    std::cout << "9. clean up the test rows\n";
    cleanup(db);
    auto left = db.execute("SELECT COUNT(*) AS n FROM users WHERE email LIKE ?", {std::string("stage3b.%@test.local")});
    CHECK(left.ok && database::rowInt(left.rows[0], "n") == 0);
    auto total = db.execute("SELECT COUNT(*) AS n FROM users");
    CHECK(total.ok && database::rowInt(total.rows[0], "n") == 7);

    if (failures == 0) std::cout << "\nSTAGE 3b OK\n";
    else std::cout << "\nSTAGE 3b FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
