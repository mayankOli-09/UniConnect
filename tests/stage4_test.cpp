// Stage 4 check: the real Server with the auth endpoints (in-memory user storage, no MySQL needed).
#include <winsock2.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "TestHttpClient.h"
#include "database/InMemoryUserRepository.h"
#include "server/Server.h"

using namespace uniconnect;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

static std::string js(const std::string& k, const std::string& v) { return "\"" + k + "\":\"" + v + "\""; }

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    const int PORT = 18081;

    database::InMemoryUserRepository repo;
    services::AuthConfig cfg;
    cfg.jwt_secret = "stage4-test-secret-0123456789";
    server::Server srv(server::ServerConfig{PORT}, server::ServerDeps{&repo}, cfg);
    std::string startError;
    std::thread th([&] { try { srv.start(); } catch (const std::exception& e) { startError = e.what(); } });
    for (int i = 0; i < 300 && !srv.isRunning() && startError.empty(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!srv.isRunning()) { std::cout << "Could not start server: " << startError << "\n"; srv.stop(); th.join(); return 1; }

    TestHttpClient c(PORT);
    CHECK(c.connected());
    TestResponse r;

    std::cout << "1. health, 404, 405, CORS preflight\n";
    CHECK(c.get("/health", r) && r.status == 200 && r.json()["status"].asString() == "ok");
    CHECK(c.get("/api/nope", r) && r.status == 404 && !r.json()["error"].asString().empty());
    CHECK(c.get("/api/auth/login", r) && r.status == 405 && r.headers["allow"] == "POST");
    CHECK(c.request("OPTIONS", "/api/auth/login", "", r, "Origin: http://localhost:5173\r\nAccess-Control-Request-Method: POST\r\n") && r.status == 204);
    CHECK(r.headers["access-control-allow-origin"] == "*" && r.headers["access-control-allow-headers"].find("Authorization") != std::string::npos);

    std::cout << "2. register a student (what the frontend sends)\n";
    const std::string regBody = "{" + js("name", "Asha Rao") + "," + js("email", "asha@example.com") + "," + js("password", "Password@123") + "," +
                                js("role", "STUDENT") + "," + js("department_id", "1") + "}";
    CHECK(c.post("/api/auth/register", regBody, r) && r.status == 201);
    auto j = r.json();
    CHECK(j["success"].asBool() && !j["token"].asString().empty());
    CHECK(j["user"]["email"].asString() == "asha@example.com" && j["user"]["role"].asString() == "STUDENT");
    CHECK(j["user"]["user_id"].asInt() > 0 && j["user"]["student_id"].asInt() > 0);
    CHECK(r.body.find("password") == std::string::npos);
    const auto claims = srv.auth().verifyToken(j["token"].asString());
    CHECK(claims && claims->user_id == j["user"]["user_id"].asInt() && claims->role == "STUDENT");

    std::cout << "3. register errors\n";
    CHECK(c.post("/api/auth/register", regBody, r) && r.status == 409);                                           // duplicate email
    CHECK(c.post("/api/auth/register", "{" + js("name", "Bob Lee") + "," + js("email", "bob@example.com") + "," + js("password", "short") + "," + js("department_id", "1") + "}", r) && r.status == 400);
    CHECK(c.post("/api/auth/register", "{" + js("name", "Eve") + "," + js("email", "eve@example.com") + "," + js("password", "Password@123") + "," + js("role", "ADMIN") + "," + js("department_id", "1") + "}", r) && r.status == 403);
    CHECK(c.post("/api/auth/register", "not json", r) && r.status == 400);
    CHECK(c.post("/api/auth/register", "[1,2,3]", r) && r.status == 400);
    CHECK(c.post("/api/auth/register", "", r) && r.status == 400);
    CHECK(c.post("/api/auth/register", "{\"name\":\"Num Dept\",\"email\":\"num@example.com\",\"password\":\"Password@123\",\"department_id\":2}", r) && r.status == 201);  // numeric id accepted
    CHECK(r.json()["user"]["department_id"].asInt() == 2);

    std::cout << "4. login\n";
    CHECK(c.post("/api/auth/login", "{" + js("email", "ASHA@example.com") + "," + js("password", "Password@123") + "}", r) && r.status == 200);
    j = r.json();
    CHECK(j["token"].asString().size() > 40 && j["user"]["name"].asString() == "Asha Rao" && j["expires_in"].asInt() > 0);
    CHECK(c.post("/api/auth/login", "{" + js("email", "asha@example.com") + "," + js("password", "wrong-password") + "}", r) && r.status == 401);
    const std::string wrongMsg = r.json()["error"].asString();
    CHECK(c.post("/api/auth/login", "{" + js("email", "ghost@example.com") + "," + js("password", "Password@123") + "}", r) && r.status == 401);
    CHECK(r.json()["error"].asString() == wrongMsg);                                                               // no account enumeration
    CHECK(c.post("/api/auth/login", "{}", r) && r.status == 400);

    std::cout << "5. lockout after repeated failures\n";
    for (int i = 0; i < 5; ++i) c.post("/api/auth/login", "{" + js("email", "num@example.com") + "," + js("password", "bad-" + std::to_string(i)) + "}", r);
    CHECK(c.post("/api/auth/login", "{" + js("email", "num@example.com") + "," + js("password", "Password@123") + "}", r) && r.status == 429);

    std::cout << "6. 8 clients logging in at the same time\n";
    std::atomic<int> ok{0};
    std::vector<std::thread> clients;
    for (int i = 0; i < 8; ++i) {
        clients.emplace_back([&] {
            TestHttpClient cc(PORT);
            TestResponse rr;
            if (cc.connected() && cc.post("/api/auth/login", "{" + js("email", "asha@example.com") + "," + js("password", "Password@123") + "}", rr) && rr.status == 200) ++ok;
        });
    }
    for (auto& t : clients) t.join();
    CHECK(ok == 8);

    std::cout << "7. oversized request body is refused, server keeps working\n";
    {
        TestHttpClient big(PORT);
        TestResponse br;
        CHECK(big.sendRaw("POST /api/auth/login HTTP/1.1\r\nHost: x\r\nContent-Length: 5000000\r\n\r\n") && big.readResponse(br) && br.status == 400);
    }
    CHECK(c.get("/health", r) && r.status == 200);

    srv.stop();
    th.join();
    WSACleanup();

    if (failures == 0) std::cout << "\nSTAGE 4 OK\n";
    else std::cout << "\nSTAGE 4 FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
