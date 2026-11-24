// Stage 6 check: groups and messages over real sockets, including permissions, validation and the send-rate limit.
#include <winsock2.h>

#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

#include "TestHttpClient.h"
#include "database/InMemoryChatRepository.h"
#include "database/InMemoryUserRepository.h"
#include "server/Server.h"

using namespace uniconnect;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

static std::string q(const std::string& s) { return "\"" + s + "\""; }

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    const int PORT = 18083;

    database::InMemoryUserRepository users;
    database::InMemoryChatRepository chat;
    services::AuthConfig cfg;
    cfg.jwt_secret = "stage6-test-secret-0123456789";
    cfg.allow_privileged_signup = true;
    server::Server srv(server::ServerConfig{PORT}, server::ServerDeps{&users, nullptr, &chat}, cfg);

    std::mutex lm;
    std::string heardText;
    std::vector<int> heardRecipients;
    srv.chat()->setMessageListener([&](const models::ChatMessage& m, const std::vector<int>& to) {
        std::lock_guard<std::mutex> l(lm);
        heardText = m.content; heardRecipients = to;
    });

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
        c.post("/api/auth/register", "{" + q("name") + ":" + q(name) + "," + q("email") + ":" + q(email) + "," + q("password") + ":" + q("Password@123") + "," +
                                         q("role") + ":" + q(role) + "," + q("department_id") + ":" + q("1") + "}", r);
        return std::make_pair(r.json()["token"].asString(), r.json()["user"]["user_id"].asInt());
    };
    const auto [t1, id1] = signUp("Asha Rao", "asha@example.com", "STUDENT");
    const auto [t2, id2] = signUp("Bala Nair", "bala@example.com", "STUDENT");
    const auto [t3, id3] = signUp("Chitra Das", "chitra@example.com", "STUDENT");
    const auto [tAdmin, idAdmin] = signUp("Admin Max", "max@example.com", "ADMIN");
    CHECK(id1 == 1 && id2 == 2 && id3 == 3);
    chat.addGroup(1, "Operating Systems - Section A", "COURSE", {id1, id2});
    chat.addGroup(2, "CSE Department", "DEPARTMENT", {id1, id2, id3});
    chat.addGroup(3, "Study circle", "GROUP", {id3});
    chat.setUserName(id1, "Asha Rao");

    std::cout << "1. sign-in required\n";
    CHECK(call("GET", "/api/groups", "", "") && r.status == 401);
    CHECK(call("GET", "/api/groups/1/messages", "", "") && r.status == 401);
    CHECK(call("POST", "/api/groups/1/messages", "{\"content\":\"hi\"}", "") && r.status == 401);
    CHECK(call("GET", "/api/groups", "", "not.a.token") && r.status == 401);

    std::cout << "2. my groups\n";
    CHECK(call("GET", "/api/groups?user_id=" + std::to_string(id1), "", t1) && r.status == 200);
    auto j = r.json();
    CHECK(j["groups"].size() == 2 && j["groups"].at(0)["group_id"].asInt() == 1 && j["groups"].at(0)["name"].asString() == "Operating Systems - Section A");
    CHECK(j["groups"].at(0)["type"].asString() == "COURSE" && j["groups"].at(0)["member_count"].asInt() == 2);
    CHECK(call("GET", "/api/groups", "", t1) && r.json()["groups"].size() == 2);                              // user_id is optional
    CHECK(call("GET", "/api/groups", "", t3) && r.json()["groups"].size() == 2);                              // Chitra: department + study circle
    CHECK(call("GET", "/api/groups?user_id=" + std::to_string(id3), "", t1) && r.status == 403);              // not somebody else's
    CHECK(call("GET", "/api/groups?user_id=" + std::to_string(id3), "", tAdmin) && r.status == 200 && r.json()["groups"].size() == 2);   // admin may
    CHECK(call("GET", "/api/groups?user_id=abc", "", t1) && r.status == 400);

    std::cout << "3. send and read\n";
    CHECK(call("GET", "/api/groups/1/messages", "", t1) && r.status == 200 && r.json()["messages"].size() == 0 && !r.json()["has_more"].asBool());
    CHECK(call("POST", "/api/groups/1/messages", "{" + q("content") + ":" + q("  Hello team  ") + "," + q("sender_name") + ":" + q("Hacker") + "," + q("priority") + ":5}", t1) && r.status == 201);
    j = r.json();
    CHECK(j["success"].asBool() && j["message"]["content"].asString() == "Hello team");                       // trimmed
    CHECK(j["message"]["sender_name"].asString() == "Asha Rao" && j["message"]["sender_id"].asInt() == id1);  // the name from the sign-in, not the request
    CHECK(j["message"]["group_id"].asInt() == 1 && !j["message"]["sent_at"].asString().empty() && j["message"]["message_id"].asInt() > 0);
    CHECK(call("POST", "/api/groups/1/messages", "{" + q("sender_id") + ":" + q(std::to_string(id1)) + "," + q("content") + ":" + q("Second \\u00e9\\ud83c\\udf93") + "}", t1) && r.status == 201);   // own sender_id is fine, unicode kept
    CHECK(call("GET", "/api/groups/1/messages", "", t2) && r.status == 200 && r.json()["messages"].size() == 2);   // Bala sees it too
    CHECK(r.json()["messages"].at(0)["content"].asString() == "Hello team" && r.json()["messages"].at(1)["content"].asString() == "Second \xC3\xA9\xF0\x9F\x8E\x93");
    {
        std::lock_guard<std::mutex> l(lm);
        CHECK(heardText == "Second \xC3\xA9\xF0\x9F\x8E\x93" && heardRecipients == std::vector<int>({id1, id2}));   // real-time hook got the right audience
    }

    std::cout << "4. only members; no impersonation\n";
    CHECK(call("GET", "/api/groups/1/messages", "", t3) && r.status == 404);                                  // Chitra is not in group 1
    CHECK(call("POST", "/api/groups/1/messages", "{\"content\":\"let me in\"}", t3) && r.status == 404);
    CHECK(call("GET", "/api/groups/99/messages", "", t1) && r.status == 404);
    CHECK(call("GET", "/api/groups/1/messages", "", tAdmin) && r.status == 404);                              // admins are not exempt either
    CHECK(call("POST", "/api/groups/1/messages", "{" + q("sender_id") + ":" + q(std::to_string(id2)) + "," + q("content") + ":" + q("as Bala") + "}", t1) && r.status == 403);
    CHECK(call("GET", "/api/groups/1/messages", "", t1) && r.json()["messages"].size() == 2);                 // the spoof was not stored

    std::cout << "5. validation\n";
    CHECK(call("POST", "/api/groups/1/messages", "{\"content\":\"   \"}", t1) && r.status == 400);
    CHECK(call("POST", "/api/groups/1/messages", "{\"other\":1}", t1) && r.status == 400);
    CHECK(call("POST", "/api/groups/1/messages", "not json", t1) && r.status == 400);
    CHECK(call("POST", "/api/groups/1/messages", "{" + q("content") + ":" + q(std::string(4001, 'x')) + "}", t1) && r.status == 400);
    CHECK(call("POST", "/api/groups/1/messages", "{" + q("content") + ":" + q(std::string(4000, 'x')) + "}", t1) && r.status == 201);   // exactly the limit
    for (const char* bad : {"0", "11", "\"high\""})
        CHECK(call("POST", "/api/groups/1/messages", std::string("{\"content\":\"hi\",\"priority\":") + bad + "}", t1) && r.status == 400);
    CHECK(call("POST", "/api/groups/abc/messages", "{\"content\":\"hi\"}", t1) && r.status == 400);

    std::cout << "6. pagination\n";
    for (int i = 1; i <= 5; ++i) call("POST", "/api/groups/2/messages", "{\"content\":\"m" + std::to_string(i) + "\"}", t1);
    CHECK(call("GET", "/api/groups/2/messages?limit=2", "", t2) && r.status == 200);
    j = r.json();
    CHECK(j["messages"].size() == 2 && j["messages"].at(0)["content"].asString() == "m4" && j["messages"].at(1)["content"].asString() == "m5" && j["has_more"].asBool());
    const int oldest = j["messages"].at(0)["message_id"].asInt();
    CHECK(call("GET", "/api/groups/2/messages?limit=10&before_id=" + std::to_string(oldest), "", t2) && r.json()["messages"].size() == 3 && !r.json()["has_more"].asBool());
    CHECK(call("GET", "/api/groups/2/messages?limit=0", "", t2) && r.status == 400);
    CHECK(call("GET", "/api/groups/2/messages?limit=201", "", t2) && r.status == 400);
    CHECK(call("GET", "/api/groups/2/messages?before_id=x", "", t2) && r.status == 400);

    std::cout << "7. spam limit: 30 messages per 10 seconds per person\n";
    int accepted = 0, refused = 0;
    for (int i = 0; i < 40; ++i) {
        call("POST", "/api/groups/2/messages", "{\"content\":\"spam " + std::to_string(i) + "\"}", t2);
        if (r.status == 201) ++accepted; else if (r.status == 429) ++refused;
    }
    CHECK(accepted == 30 && refused == 10);
    CHECK(call("POST", "/api/groups/2/messages", "{\"content\":\"hi\"}", t3) && r.status == 201);              // other people are unaffected

    srv.stop();
    th.join();
    WSACleanup();
    if (failures == 0) std::cout << "\nSTAGE 6 OK\n";
    else std::cout << "\nSTAGE 6 FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
