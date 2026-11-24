// Stage 11 check: admin notices to students, faculty or both, with live pop-ups and the notification "kind".
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
static int countKind(const JsonValue& list, const std::string& kind) {
    int n = 0;
    for (size_t i = 0; i < list.size(); ++i) if (list.at(i)["kind"].asString() == kind) ++n;
    return n;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    const int PORT = 18091;

    database::InMemoryUserRepository users;
    database::InMemoryChatRepository chat;
    database::InMemoryNotificationRepository notes(chat);
    services::AuthConfig cfg;
    cfg.jwt_secret = "stage11-test-secret-0123456789";
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
        const int id = r.json()["user"]["user_id"].asInt();
        notes.setRole(id, role);
        return std::make_pair(r.json()["token"].asString(), id);
    };
    const auto [tM, idM] = signUp("Admin Max", "max@example.com", "ADMIN");
    const auto [tF, idF] = signUp("Prof Fay", "fay@example.com", "FACULTY");
    const auto [tA, idA] = signUp("Asha Rao", "asha@example.com", "STUDENT");
    const auto [tB, idB] = signUp("Bala Nair", "bala@example.com", "STUDENT");
    const auto [tC, idC] = signUp("Chitra Das", "chitra@example.com", "STUDENT");
    chat.addGroup(1, "OS - Section A", "COURSE", {idF, idA, idB});
    auto notice = [&](const std::string& token, const std::string& audience, const std::string& title, const std::string& content) {
        return call("POST", "/api/notices", "{" + q("audience") + ":" + q(audience) + "," + q("title") + ":" + q(title) + "," + q("content") + ":" + q(content) + "}", token);
    };
    auto authFrame = [](const std::string& t) { return "{\"action\":\"authenticate\",\"token\":\"" + t + "\"}"; };

    TestWsClient wa(PORT), wf(PORT), wm(PORT);
    JsonValue ev;
    CHECK(wa.handshake() && wa.sendText(authFrame(tA)) && wa.waitEvent("authenticated", ev));
    CHECK(wf.handshake() && wf.sendText(authFrame(tF)) && wf.waitEvent("authenticated", ev));
    CHECK(wm.handshake() && wm.sendText(authFrame(tM)) && wm.waitEvent("authenticated", ev));

    std::cout << "1. only an administrator can send a notice\n";
    CHECK(notice("", "BOTH", "t", "c") && r.status == 401);
    CHECK(notice(tA, "BOTH", "t", "c") && r.status == 403);
    CHECK(notice(tF, "BOTH", "t", "c") && r.status == 403);

    std::cout << "2. a notice to students reaches every student and nobody else\n";
    const auto done0 = srv.dispatcher().scheduler().getMetrics().total_tasks_completed;
    CHECK(notice(tM, "STUDENTS", "  Exam timetable  ", "Exams start on Monday.") && r.status == 201);
    CHECK(r.json()["success"].asBool() && r.json()["recipients"].asInt() == 3 && r.json()["message"].asString() == "Notice sent to 3 people.");
    CHECK(srv.dispatcher().scheduler().getMetrics().total_tasks_completed > done0);              // it ran as a scheduled task
    CHECK(wa.waitEvent("notification", ev) && ev["data"]["kind"].asString() == "NOTICE" && ev["data"]["title"].asString() == "Exam timetable" && !ev["data"]["is_read"].asBool());
    CHECK(!wf.waitEvent("notification", ev, 400));                                               // faculty were not addressed
    CHECK(!wm.waitEvent("notification", ev, 200));                                               // the sender is not notified
    CHECK(call("GET", "/api/notifications", "", tA) && r.json()["notifications"].size() == 1 && r.json()["notifications"].at(0)["kind"].asString() == "NOTICE" && r.json()["unread_count"].asInt() == 1);
    CHECK(call("GET", "/api/notifications", "", tC) && r.json()["notifications"].size() == 1);   // offline students get it too
    CHECK(call("GET", "/api/notifications", "", tF) && r.json()["notifications"].size() == 0);

    std::cout << "3. a notice to faculty\n";
    CHECK(notice(tM, "FACULTY", "Staff meeting", "Friday 4 pm, Room 2.") && r.status == 201 && r.json()["recipients"].asInt() == 1);
    CHECK(wf.waitEvent("notification", ev) && ev["data"]["title"].asString() == "Staff meeting");
    CHECK(!wa.waitEvent("notification", ev, 400));
    CHECK(call("GET", "/api/notifications", "", tF) && r.json()["notifications"].size() == 1 && r.json()["notifications"].at(0)["kind"].asString() == "NOTICE");

    std::cout << "4. a notice to both\n";
    CHECK(notice(tM, "BOTH", "Holiday", "The campus is closed on Friday.") && r.status == 201 && r.json()["recipients"].asInt() == 4);
    CHECK(wa.waitEvent("notification", ev) && ev["data"]["title"].asString() == "Holiday");
    CHECK(wf.waitEvent("notification", ev) && ev["data"]["title"].asString() == "Holiday");
    CHECK(call("GET", "/api/notifications", "", tM) && r.json()["notifications"].size() == 0);   // the admin who sent them gets nothing

    std::cout << "5. notices and group announcements keep their own kind\n";
    CHECK(call("POST", "/api/announcements", "{\"group_id\":\"1\",\"title\":\"Lab moved\",\"content\":\"Room 5\"}", tF) && r.status == 201);
    CHECK(call("GET", "/api/notifications", "", tA) && r.json()["notifications"].size() == 3);
    CHECK(countKind(r.json()["notifications"], "NOTICE") == 2 && countKind(r.json()["notifications"], "ANNOUNCEMENT") == 1);

    std::cout << "6. validation (the audience may be written in any case)\n";
    CHECK(notice(tM, "EVERYONE", "t", "c") && r.status == 400);
    CHECK(notice(tM, "", "t", "c") && r.status == 400);
    CHECK(notice(tM, "BOTH", "   ", "c") && r.status == 400);
    CHECK(notice(tM, "BOTH", "t", "   ") && r.status == 400);
    CHECK(notice(tM, "BOTH", std::string(201, 'x'), "c") && r.status == 400);
    CHECK(notice(tM, "BOTH", "t", std::string(4001, 'x')) && r.status == 400);
    CHECK(call("POST", "/api/notices", "not json", tM) && r.status == 400);
    CHECK(call("POST", "/api/notices", "{}", tM) && r.status == 400);
    CHECK(notice(tM, "students", "lower case", "is fine") && r.status == 201 && r.json()["recipients"].asInt() == 3);

    std::cout << "7. spam limit: 10 per minute, shared with announcements\n";
    int ok = 0, refused = 0;
    for (int i = 0; i < 10; ++i) {
        notice(tM, "FACULTY", "Spam " + std::to_string(i), "content");
        if (r.status == 201) ++ok; else if (r.status == 429) ++refused;
    }
    CHECK(ok == 6 && refused == 4);        // four notices were already sent by this admin; validation failures do not count

    srv.stop();
    th.join();
    WSACleanup();
    if (failures == 0) std::cout << "\nSTAGE 11 OK\n";
    else std::cout << "\nSTAGE 11 FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
