// Stage 5b check: enroll / drop and automatic group membership against the real MySQL database.
#include <iostream>
#include <string>

#include "database/MySqlAcademicRepository.h"
#include "database/MySqlUserRepository.h"
#include "services/AcademicService.h"
#include "services/AuthService.h"
#include "utils/Crypto.h"

using namespace uniconnect;
using database::rowInt;
using database::rowStr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

static database::MySqlDatabase* g_db = nullptr;
static int count(const std::string& sql, const std::vector<database::DbValue>& p = {}) {
    const auto r = g_db->execute(sql, p);
    return r.ok && !r.rows.empty() ? rowInt(r.rows[0], "n") : -1;
}
static bool isMember(int conv, int user) {
    return count("SELECT COUNT(*) AS n FROM conversation_members WHERE conversation_id = ? AND user_id = ?", {std::to_string(conv), std::to_string(user)}) == 1;
}
static int convId(const std::string& type, const database::DbValue& course, const database::DbValue& section, const database::DbValue& dept) {
    const auto r = g_db->execute("SELECT conversation_id AS n FROM conversations WHERE conversation_type = ? AND course_id <=> ? AND section_id <=> ? AND department_id <=> ? LIMIT 1", {type, course, section, dept});
    return r.ok && !r.rows.empty() ? rowInt(r.rows[0], "n") : 0;
}

int main() {
    database::DbConfig cfg = database::DbConfig::fromEnvironment();
    if (cfg.password.empty()) cfg.password = "UniConnect#Dev2026";   // documented local development password
    database::MySqlDatabase db(cfg);
    g_db = &db;
    std::string err;
    if (!db.connect(err)) { std::cout << "CANNOT CONNECT: " << err << "\n"; return 1; }

    database::MySqlUserRepository users(db);
    database::MySqlAcademicRepository repo(db);
    services::AcademicService acad(repo);
    services::AuthConfig ac;
    ac.jwt_secret = "stage5b-test-secret-0123456789";
    services::AuthService auth(users, ac);

    const int usersBefore = count("SELECT COUNT(*) AS n FROM users");
    const int enrollBefore = count("SELECT COUNT(*) AS n FROM enrollments");
    const int convBefore = count("SELECT COUNT(*) AS n FROM conversations");
    const int maxConvBefore = count("SELECT MAX(conversation_id) AS n FROM conversations");

    std::cout << "1. courses and hierarchy from the real tables\n";
    auto courses = acad.courses();
    CHECK(courses["courses"].size() == 4 && courses["courses"].at(0)["course_code"].asString() == "CS301");
    auto tree = acad.hierarchy();
    CHECK(tree["departments"].size() == 3);
    const auto& cse = tree["departments"].at(0);
    CHECK(cse["name"].asString() == "Computer Science and Engineering" && cse["programs"].at(0)["sections"].size() == 2);
    CHECK(cse["programs"].at(0)["sections"].at(0)["courses"].size() == 4);
    CHECK(tree["departments"].at(1)["programs"].at(0)["sections"].at(0)["courses"].size() == 0);   // Mechanical has no courses yet

    std::cout << "2. create a test student (home section 1, CSE)\n";
    const std::string tag = utils::crypto::toHex(utils::crypto::randomBytes(4));
    const auto reg = auth.registerUser("Enroll Tester", "stage5b." + tag + "@test.local", "Password@123", models::UserRole::STUDENT, "1");
    CHECK(reg.success && reg.user.student_id > 0 && reg.user.section_id == 1);
    const int sid = reg.user.student_id, uid = reg.user.id;
    const auto student = reg.user;
    models::User admin; admin.id = 7; admin.role = models::UserRole::ADMIN;
    models::User prof; prof.id = 5; prof.role = models::UserRole::FACULTY;
    const int deptConv = convId("DEPARTMENT", std::nullopt, std::nullopt, std::string("1"));
    const int sectionConv = convId("SECTION", std::nullopt, std::string("1"), std::nullopt);
    CHECK(deptConv > 0 && sectionConv > 0);
    CHECK(!isMember(deptConv, uid) && !isMember(sectionConv, uid));                       // not in any group yet

    std::cout << "3. enroll in a course that has no group yet (CS304, section A)\n";
    CHECK(convId("COURSE", std::string("4"), std::string("1"), std::nullopt) == 0);
    auto r = acad.enroll(student, "", "1", "4");
    CHECK(r.status == 200 && r.body["success"].asBool() && r.body["groups"].size() == 3);
    std::cout << "   " << r.body["message"].asString() << "\n";
    const int courseConv = convId("COURSE", std::string("4"), std::string("1"), std::nullopt);
    CHECK(courseConv > 0);                                                                 // the group was generated
    CHECK(count("SELECT COUNT(*) AS n FROM conversations WHERE conversation_id = ? AND conversation_name = 'Artificial Intelligence - Section A'", {std::to_string(courseConv)}) == 1);
    CHECK(isMember(courseConv, uid) && isMember(sectionConv, uid) && isMember(deptConv, uid));
    CHECK(count("SELECT COUNT(*) AS n FROM enrollments WHERE student_id = ? AND course_id = 4 AND status = 'ACTIVE'", {std::to_string(sid)}) == 1);
    CHECK(count("SELECT COUNT(*) AS n FROM conversations") == convBefore + 1);             // only the missing group was created

    std::cout << "4. rejected requests change nothing\n";
    CHECK(acad.enroll(student, "", "1", "4").status == 409);                               // already enrolled
    CHECK(acad.enroll(student, "", "1", "").status == 400);                                // section offers four courses: must choose
    CHECK(acad.enroll(student, "", "3", "1").status == 400);                               // CS301 is not a Mechanical course
    CHECK(acad.enroll(student, "", "3", "").status == 400);                                // Mechanical section has no courses
    CHECK(acad.enroll(student, "", "999", "1").status == 404);
    CHECK(acad.enroll(student, "", "1", "999").status == 404);
    CHECK(acad.enroll(admin, "999999", "1", "1").status == 404);                           // unknown student
    CHECK(acad.enroll(student, "1", "1", "1").status == 403);                              // student 1 is somebody else
    CHECK(acad.enroll(prof, std::to_string(sid), "1", "1").status == 403);
    CHECK(count("SELECT COUNT(*) AS n FROM enrollments") == enrollBefore + 1);
    CHECK(count("SELECT COUNT(*) AS n FROM conversations") == convBefore + 1);

    std::cout << "5. an admin can enroll the student in a second course; groups are reused, not duplicated\n";
    r = acad.enroll(admin, std::to_string(sid), "1", "3");
    CHECK(r.status == 200);
    CHECK(count("SELECT COUNT(*) AS n FROM conversations WHERE conversation_type = 'SECTION' AND section_id = 1") == 1);
    CHECK(count("SELECT COUNT(*) AS n FROM conversation_members WHERE user_id = ?", {std::to_string(uid)}) == 4);   // CS304 group + CS303 group + section + department
    const int cnGroup = convId("COURSE", std::string("3"), std::string("1"), std::nullopt);
    CHECK(cnGroup > 0 && cnGroup != courseConv && isMember(cnGroup, uid));

    std::cout << "6. drop: leaves the course group but stays in the home-section and department groups\n";
    r = acad.drop(student, "", "1", "4");
    CHECK(r.status == 200 && r.body["groups"].size() == 1);
    std::cout << "   " << r.body["message"].asString() << "\n";
    CHECK(!isMember(courseConv, uid) && isMember(sectionConv, uid) && isMember(deptConv, uid) && isMember(cnGroup, uid));
    CHECK(count("SELECT COUNT(*) AS n FROM enrollments WHERE student_id = ? AND course_id = 4 AND status = 'DROPPED'", {std::to_string(sid)}) == 1);
    CHECK(acad.drop(student, "", "1", "4").status == 404);                                 // nothing left to drop
    CHECK(acad.drop(student, "", "1", "").status == 200);                                  // only CS303 is active now, so no course_id needed
    CHECK(!isMember(cnGroup, uid));
    CHECK(acad.drop(prof, std::to_string(sid), "1", "1").status == 403);

    std::cout << "7. enrolling again re-activates the same record\n";
    CHECK(acad.enroll(student, "", "1", "4").status == 200);
    CHECK(isMember(courseConv, uid));
    CHECK(count("SELECT COUNT(*) AS n FROM enrollments WHERE student_id = ? AND course_id = 4", {std::to_string(sid)}) == 1);

    std::cout << "8. a different section: groups for it are generated, and dropping removes the section group too\n";
    r = acad.enroll(student, "", "2", "1");
    CHECK(r.status == 200);
    const int bCourse = convId("COURSE", std::string("1"), std::string("2"), std::nullopt);
    const int bSection = convId("SECTION", std::nullopt, std::string("2"), std::nullopt);
    CHECK(bCourse > 0 && bSection > 0 && isMember(bCourse, uid) && isMember(bSection, uid));
    r = acad.drop(student, "", "2", "1");
    CHECK(r.status == 200 && r.body["groups"].size() == 2);
    CHECK(!isMember(bCourse, uid) && !isMember(bSection, uid) && isMember(deptConv, uid));

    std::cout << "9. clean up\n";
    db.execute("DELETE FROM conversation_members WHERE user_id = ?", {std::to_string(uid)});
    db.execute("DELETE FROM enrollments WHERE student_id = ?", {std::to_string(sid)});
    db.execute("DELETE FROM conversations WHERE conversation_id > ?", {std::to_string(maxConvBefore)});
    db.execute("DELETE FROM students WHERE user_id = ?", {std::to_string(uid)});
    db.execute("DELETE FROM users WHERE user_id = ?", {std::to_string(uid)});
    CHECK(count("SELECT COUNT(*) AS n FROM users") == usersBefore);
    CHECK(count("SELECT COUNT(*) AS n FROM enrollments") == enrollBefore);
    CHECK(count("SELECT COUNT(*) AS n FROM conversations") == convBefore);

    if (failures == 0) std::cout << "\nSTAGE 5b OK\n";
    else std::cout << "\nSTAGE 5b FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
