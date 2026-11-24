// Stage 8 check: announcements, notifications, live push and mark-as-read over real sockets.
#include <winsock2.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "TestHttpClient.h"
#include "TestWsClient.h"
#include "database/InMemoryChatRepository.h"
#include "database/InMemoryNotificationRepository.h"
#include "database/InMemoryUserRepository.h"
#include "server/Server.h"

using namespace uniconnect;
using utils::JsonValue;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

static std::string q(const std::string& s) { return "\"" + s + "\""; }
static void pause(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    const int PORT = 18085;

    database::InMemoryUserRepository users;
    database::InMemoryChatRepository chat;
    database::InMemoryNotificationRepository notes(chat);
    services::AuthConfig cfg;
    cfg.jwt_secret = "stage8-test-secret-0123456789";
    cfg.allow_privileged_signup = true;
    server::Server srv(server::ServerConfig{PORT}, server::ServerDeps{&users, nullptr, &chat, &notes}, cfg);
    std::string startError;
    std::thread th([&] { try { srv.start(); } catch (const std::exception& e) { startError = e.what(); } });
    for (int i = 0; i < 300 && !srv.isRunning() && startError.empty(); ++i) pause(10);
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
    const auto [tF, idF] = signUp("Prof Fay", "fay@example.com", "FACULTY");
    const auto [tA, idA] = signUp("Asha Rao", "asha@example.com", "STUDENT");
    const auto [tB, idB] = signUp("Bala Nair", "bala@example.com", "STUDENT");
    const auto [tC, idC] = signUp("Chitra Das", "chitra@example.com", "STUDENT");
    const auto [tM, idM] = signUp("Admin Max", "max@example.com", "ADMIN");
    chat.addGroup(1, "OS - Section A", "COURSE", {idF, idA, idB});
    chat.addGroup(2, "CSE Department", "DEPARTMENT", {idA, idB, idC});
    auto announce = [&](const std::string& token, const std::string& group, const std::string& title, const std::string& content, const std::string& extra = "") {
        return call("POST", "/api/announcements", "{" + q("group_id") + ":" + q(group) + "," + q("title") + ":" + q(title) + "," + q("content") + ":" + q(content) + extra + "}", token);
    };

    std::cout << "1. sign-in required, and the list starts empty\n";
    CHECK(call("GET", "/api/notifications", "", "") && r.status == 401);
    CHECK(call("POST", "/api/announcements", "{}", "") && r.status == 401);
    CHECK(call("POST", "/api/notifications/read", "{}", "") && r.status == 401);
    CHECK(call("GET", "/api/notifications?user_id=" + std::to_string(idA), "", tA) && r.status == 200 && r.json()["notifications"].size() == 0 && r.json()["unread_count"].asInt() == 0);

    TestWsClient wa(PORT), wb(PORT), wc(PORT);
    JsonValue ev;
    CHECK(wa.handshake() && wa.sendText("{\"action\":\"authenticate\",\"token\":\"" + tA + "\"}") && wa.waitEvent("authenticated", ev));
    CHECK(wb.handshake() && wb.sendText("{\"action\":\"authenticate\",\"token\":\"" + tB + "\"}") && wb.waitEvent("authenticated", ev));
    CHECK(wc.handshake() && wc.sendText("{\"action\":\"authenticate\",\"token\":\"" + tC + "\"}") && wc.waitEvent("authenticated", ev));

    std::cout << "2. who may post\n";
    CHECK(announce(tA, "1", "Hi", "I am a student") && r.status == 403);
    CHECK(announce(tF, "2", "Hi", "Fay is not in group 2") && r.status == 404);          // faculty only announce to their own groups
    CHECK(announce(tF, "99", "Hi", "no such group") && r.status == 404);
    CHECK(announce(tM, "99", "Hi", "no such group") && r.status == 404);

    std::cout << "3. a faculty announcement is stored, scheduled, and pushed live\n";
    const auto doneBefore = srv.dispatcher().scheduler().getMetrics().total_tasks_completed;
    CHECK(announce(tF, "1", "  Internal test  ", "Monday, 10 am, Lab 3", ",\"priority\":8") && r.status == 201);
    CHECK(r.json()["success"].asBool() && r.json()["recipients"].asInt() == 2 && r.json()["announcement_id"].asInt() > 0);
    CHECK(r.json()["message"].asString() == "Announcement sent to 2 people.");
    CHECK(srv.dispatcher().scheduler().getMetrics().total_tasks_completed > doneBefore);   // it ran as a scheduled task
    CHECK(wa.waitEvent("notification", ev) && ev["data"]["title"].asString() == "Internal test" && ev["data"]["content"].asString() == "Monday, 10 am, Lab 3");
    CHECK(ev["data"]["is_read"].asBool() == false && ev["data"]["announcement_id"].asInt() > 0);
    CHECK(wb.waitEvent("notification", ev));
    CHECK(!wc.waitEvent("notification", ev, 400));                                             // Chitra is not in the group

    std::cout << "4. everyone sees their own copy\n";
    CHECK(call("GET", "/api/notifications", "", tA) && r.status == 200 && r.json()["notifications"].size() == 1 && r.json()["unread_count"].asInt() == 1);
    const int idNoteA = r.json()["notifications"].at(0)["notification_id"].asInt();
    CHECK(call("GET", "/api/notifications?user_id=" + std::to_string(idB), "", tB) && r.json()["notifications"].size() == 1);
    const int idNoteB = r.json()["notifications"].at(0)["notification_id"].asInt();
    CHECK(call("GET", "/api/notifications", "", tF) && r.json()["notifications"].size() == 0);   // the sender is not notified
    CHECK(call("GET", "/api/notifications", "", tC) && r.json()["notifications"].size() == 0);
    CHECK(call("GET", "/api/notifications?user_id=" + std::to_string(idB), "", tA) && r.status == 403);
    CHECK(call("GET", "/api/notifications?user_id=" + std::to_string(idB), "", tM) && r.status == 200 && r.json()["notifications"].size() == 1);   // admin may
    CHECK(call("GET", "/api/notifications?user_id=abc", "", tA) && r.status == 400);

    std::cout << "5. marking as read\n";
    CHECK(call("POST", "/api/notifications/read", "{\"ids\":[" + std::to_string(idNoteB) + "]}", tA) && r.status == 200 && r.json()["updated"].asInt() == 0);   // Bala's, not Asha's
    CHECK(call("GET", "/api/notifications", "", tB) && r.json()["unread_count"].asInt() == 1);
    CHECK(call("POST", "/api/notifications/read", "{\"ids\":[" + std::to_string(idNoteA) + "]}", tA) && r.json()["updated"].asInt() == 1 && r.json()["unread_count"].asInt() == 0);
    CHECK(call("GET", "/api/notifications", "", tA) && r.json()["notifications"].at(0)["is_read"].asBool());
    CHECK(call("POST", "/api/notifications/read", "{\"all\":true}", tB) && r.json()["updated"].asInt() == 1 && r.json()["unread_count"].asInt() == 0);
    CHECK(call("POST", "/api/notifications/read", "{\"all\":true}", tB) && r.json()["updated"].asInt() == 0);   // nothing left
    CHECK(call("POST", "/api/notifications/read", "{\"nothing\":1}", tA) && r.status == 400);
    CHECK(call("POST", "/api/notifications/read", "{\"ids\":[\"x\"]}", tA) && r.status == 400);

    std::cout << "6. validation\n";
    CHECK(announce(tF, "1", "   ", "content") && r.status == 400);
    CHECK(announce(tF, "1", "title", "   ") && r.status == 400);
    CHECK(announce(tF, "1", std::string(201, 'x'), "content") && r.status == 400);
    CHECK(announce(tF, "1", "title", std::string(4001, 'x')) && r.status == 400);
    CHECK(announce(tF, "1", "title", "content", ",\"priority\":0") && r.status == 400);
    CHECK(announce(tF, "abc", "title", "content") && r.status == 400);
    CHECK(call("POST", "/api/announcements", "not json", tF) && r.status == 400);

    std::cout << "7. an admin can announce to any group\n";
    CHECK(announce(tM, "2", "Holiday", "Campus is closed on Friday") && r.status == 201 && r.json()["recipients"].asInt() == 3);
    CHECK(wc.waitEvent("notification", ev) && ev["data"]["title"].asString() == "Holiday");

    std::cout << "8. announcement spam limit (10 per minute)\n";
    int ok = 0, refused = 0;
    for (int i = 0; i < 14; ++i) {
        announce(tF, "1", "Spam " + std::to_string(i), "content");
        if (r.status == 201) ++ok; else if (r.status == 429) ++refused;
    }
    CHECK(ok == 9 && refused == 5);    // one announcement was already used in step 3; validation failures do not count

    srv.stop();
    th.join();
    WSACleanup();
    if (failures == 0) std::cout << "\nSTAGE 8 OK\n";
    else std::cout << "\nSTAGE 8 FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
