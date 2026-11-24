// Stage 5 check: courses, hierarchy, enroll/drop over real sockets, including who is allowed to do what.
#include <winsock2.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "TestHttpClient.h"
#include "database/InMemoryAcademicRepository.h"
#include "database/InMemoryUserRepository.h"
#include "server/Server.h"

using namespace uniconnect;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

static std::string q(const std::string& s) { return "\"" + s + "\""; }

struct Account { std::string token; int student_id = 0; int user_id = 0; };

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    const int PORT = 18082;

    database::InMemoryUserRepository users;
    database::InMemoryAcademicRepository academics;
    services::AuthConfig cfg;
    cfg.jwt_secret = "stage5-test-secret-0123456789";
    cfg.allow_privileged_signup = true;
    server::Server srv(server::ServerConfig{PORT}, server::ServerDeps{&users, &academics}, cfg);
    std::string startError;
    std::thread th([&] { try { srv.start(); } catch (const std::exception& e) { startError = e.what(); } });
    for (int i = 0; i < 300 && !srv.isRunning() && startError.empty(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!srv.isRunning()) { std::cout << "Could not start server: " << startError << "\n"; srv.stop(); th.join(); return 1; }

    TestHttpClient c(PORT);
    TestResponse r;
    auto call = [&](const std::string& m, const std::string& path, const std::string& body, const std::string& token) {
        return c.request(m, path, body, r, token.empty() ? "" : "Authorization: Bearer " + token + "\r\n");
    };
    auto signUp = [&](const std::string& name, const std::string& email, const std::string& role) {
        Account a;
        c.post("/api/auth/register", "{" + q("name") + ":" + q(name) + "," + q("email") + ":" + q(email) + "," + q("password") + ":" + q("Password@123") + "," +
                                         q("role") + ":" + q(role) + "," + q("department_id") + ":" + q("1") + "}", r);
        const auto j = r.json();
        a.token = j["token"].asString();
        a.student_id = j["user"]["student_id"].asInt();
        a.user_id = j["user"]["user_id"].asInt();
        return a;
    };

    std::cout << "1. every endpoint requires a signed-in user\n";
    CHECK(call("GET", "/api/courses", "", "") && r.status == 401);
    CHECK(call("GET", "/api/hierarchy", "", "") && r.status == 401);
    CHECK(call("POST", "/api/enroll", "{}", "") && r.status == 401);
    CHECK(call("POST", "/api/drop", "{}", "") && r.status == 401);
    CHECK(call("GET", "/api/courses", "", "abc.def.ghi") && r.status == 401);
    {
        auth::JwtService other("a-completely-different-secret");
        CHECK(call("GET", "/api/courses", "", other.issue(1, "ADMIN", 3600)) && r.status == 401);      // forged with the wrong secret
    }
    CHECK(c.request("OPTIONS", "/api/enroll", "", r, "Origin: http://localhost:5173\r\n") && r.status == 204);   // preflight needs no token

    const Account a = signUp("Asha Rao", "asha@example.com", "STUDENT");
    const Account b = signUp("Bala Nair", "bala@example.com", "STUDENT");
    const Account f = signUp("Prof Fay", "fay@example.com", "FACULTY");
    const Account m = signUp("Admin Max", "max@example.com", "ADMIN");
    CHECK(a.token.size() > 40 && a.student_id > 0 && b.student_id > 0 && a.student_id != b.student_id && f.student_id == 0);

    std::cout << "2. courses and hierarchy\n";
    CHECK(call("GET", "/api/courses", "", a.token) && r.status == 200);
    auto j = r.json();
    CHECK(j["courses"].size() == 2 && j["courses"].at(0)["course_code"].asString() == "CS301" && j["courses"].at(0)["credits"].asInt() == 4);
    CHECK(call("GET", "/api/hierarchy", "", a.token) && r.status == 200);
    j = r.json();
    const auto& sec = j["departments"].at(0)["programs"].at(0)["sections"].at(0);
    CHECK(j["name"].asString() == "University" && j["departments"].size() == 2);
    CHECK(sec["section_id"].asInt() == 1 && sec["courses"].size() == 2 && sec["name"].asString().find("Section A") != std::string::npos);

    std::cout << "3. enrolling\n";
    const std::string enrollA1 = "{" + q("section_id") + ":" + q("1") + "," + q("course_id") + ":" + q("1") + "}";   // student_id omitted: the caller
    CHECK(call("POST", "/api/enroll", enrollA1, a.token) && r.status == 200 && r.json()["success"].asBool() && r.json()["groups"].size() == 3);
    CHECK(call("POST", "/api/enroll", enrollA1, a.token) && r.status == 409);                                    // already enrolled
    CHECK(call("POST", "/api/enroll", "{" + q("student_id") + ":" + std::to_string(a.student_id) + "," + q("section_id") + ":1," + q("course_id") + ":2}", a.token) && r.status == 200);   // numbers work too
    CHECK(call("POST", "/api/enroll", "{" + q("student_id") + ":" + q(std::to_string(b.student_id)) + "," + q("section_id") + ":" + q("1") + "," + q("course_id") + ":" + q("1") + "}", a.token) && r.status == 403);   // someone else's
    CHECK(call("POST", "/api/enroll", enrollA1, f.token) && r.status == 403);                                    // faculty cannot enroll
    CHECK(call("POST", "/api/enroll", "{" + q("student_id") + ":" + q(std::to_string(b.student_id)) + "," + q("section_id") + ":" + q("1") + "," + q("course_id") + ":" + q("2") + "}", m.token) && r.status == 200);   // admin can
    CHECK(call("POST", "/api/enroll", enrollA1, m.token) && r.status == 400);                                    // admin must name the student
    CHECK(call("POST", "/api/enroll", "{" + q("section_id") + ":" + q("abc") + "}", a.token) && r.status == 400);
    CHECK(call("POST", "/api/enroll", "{" + q("section_id") + ":" + q("1") + "," + q("course_id") + ":" + q("x") + "}", a.token) && r.status == 400);
    CHECK(call("POST", "/api/enroll", "{" + q("section_id") + ":" + q("999") + "," + q("course_id") + ":" + q("1") + "}", a.token) && r.status == 404);
    CHECK(call("POST", "/api/enroll", "{" + q("section_id") + ":" + q("3") + "," + q("course_id") + ":" + q("1") + "}", a.token) && r.status == 400);
    CHECK(call("POST", "/api/enroll", "{" + q("section_id") + ":" + q("2") + "}", a.token) && r.status == 400 && r.json()["error"].asString().find("Choose one") != std::string::npos);
    CHECK(call("POST", "/api/enroll", "not json", a.token) && r.status == 400);

    std::cout << "4. dropping\n";
    CHECK(call("POST", "/api/drop", enrollA1, a.token) && r.status == 200 && r.json()["success"].asBool());
    CHECK(call("POST", "/api/drop", enrollA1, a.token) && r.status == 404);                                      // already dropped
    CHECK(call("POST", "/api/drop", enrollA1, f.token) && r.status == 403);
    CHECK(call("POST", "/api/enroll", enrollA1, a.token) && r.status == 200);                                    // can enroll again

    std::cout << "5. other routes are unaffected\n";
    CHECK(c.get("/health", r) && r.status == 200);
    CHECK(call("GET", "/api/enroll", "", a.token) && r.status == 405);

    std::cout << "6. hierarchy builder\n";
    database::AcademicData d;
    d.departments = {{7, "Physics"}, {8, "Maths"}};
    d.programs = {{1, "BSc Physics", 7}, {2, "BSc Maths", 8}};
    d.sections = {{10, "A", 1, 1, "2026-27"}, {11, "B", 1, 1, "2026-27"}, {12, "A", 2, 2, "2026-27"}};
    d.courses = {{5, "PH101", "Mechanics", 3, 7, "Physics"}, {6, "MA101", "Calculus", 4, 8, "Maths"}, {9, "PH102", "Optics", 3, 7, "Physics"}};
    const auto tree = services::AcademicService::buildHierarchy(d);
    CHECK(tree["departments"].size() == 2);
    CHECK(tree["departments"].at(0)["programs"].at(0)["sections"].size() == 2);
    CHECK(tree["departments"].at(0)["programs"].at(0)["sections"].at(1)["courses"].size() == 2);   // Physics sections offer both Physics courses
    CHECK(tree["departments"].at(1)["programs"].at(0)["sections"].at(0)["courses"].at(0)["course_code"].asString() == "MA101");

    srv.stop();
    th.join();
    WSACleanup();
    if (failures == 0) std::cout << "\nSTAGE 5 OK\n";
    else std::cout << "\nSTAGE 5 FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
