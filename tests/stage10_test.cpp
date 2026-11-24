// Stage 10 check: people directory, direct chats, user-made groups, mute, unread/read, typing and online status.
#include <winsock2.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "TestHttpClient.h"
#include "TestWsClient.h"
#include "database/InMemoryChatRepository.h"
#include "database/InMemoryUserRepository.h"
#include "server/Server.h"

using namespace uniconnect;
using utils::JsonValue;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

static std::string q(const std::string& s) { return "\"" + s + "\""; }
static void pause(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
static std::string num(int n) { return std::to_string(n); }

// the entry with the given id (key "group_id" or "user_id") from a list, or null
static JsonValue findIn(const JsonValue& list, const std::string& key, int id) {
    for (size_t i = 0; i < list.size(); ++i) if (list.at(i)[key].asInt() == id) return list.at(i);
    return JsonValue();
}
static bool waitPresence(TestWsClient& c, int userId, bool online) {
    JsonValue ev;
    for (int i = 0; i < 12; ++i) {
        if (!c.waitEvent("presence", ev, 2000)) return false;
        if (ev["data"]["user_id"].asInt() == userId && ev["data"]["online"].asBool() == online) return true;
    }
    return false;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    const int PORT = 18090;

    database::InMemoryUserRepository users;
    database::InMemoryChatRepository chat;
    services::AuthConfig cfg;
    cfg.jwt_secret = "stage10-test-secret-0123456789";
    server::Server srv(server::ServerConfig{PORT}, server::ServerDeps{&users, nullptr, &chat}, cfg);
    std::string startError;
    std::thread th([&] { try { srv.start(); } catch (const std::exception& e) { startError = e.what(); } });
    for (int i = 0; i < 300 && !srv.isRunning() && startError.empty(); ++i) pause(10);
    if (!srv.isRunning()) { std::cout << "Could not start server: " << startError << "\n"; srv.stop(); th.join(); return 1; }

    TestHttpClient c(PORT);
    TestResponse r;
    auto call = [&](const std::string& m, const std::string& path, const std::string& body, const std::string& token) {
        return c.request(m, path, body, r, token.empty() ? "" : "Authorization: Bearer " + token + "\r\n");
    };
    auto signUp = [&](const std::string& name, const std::string& email) {
        c.post("/api/auth/register", "{" + q("name") + ":" + q(name) + "," + q("email") + ":" + q(email) + "," + q("password") + ":" + q("Password@123") + "," + q("department_id") + ":" + q("1") + "}", r);
        const int id = r.json()["user"]["user_id"].asInt();
        chat.addUser(id, name, "STUDENT");
        return std::make_pair(r.json()["token"].asString(), id);
    };
    const auto [tA, idA] = signUp("Asha Rao", "asha@example.com");
    const auto [tB, idB] = signUp("Bala Nair", "bala@example.com");
    const auto [tC, idC] = signUp("Chitra Das", "chitra@example.com");
    const auto [tD, idD] = signUp("Dev Iyer", "dev@example.com");
    chat.addGroup(1, "OS - Section A", "COURSE", {idA, idB, idC});
    auto authFrame = [](const std::string& t) { return "{\"action\":\"authenticate\",\"token\":\"" + t + "\"}"; };
    auto subFrame = [](int g) { return "{\"action\":\"subscribe\",\"group_id\":" + std::to_string(g) + "}"; };
    auto connect = [&](const std::string& token) {
        auto w = std::make_unique<TestWsClient>(PORT);
        JsonValue ev;
        const bool ok = w->handshake() && w->sendText(authFrame(token)) && w->waitEvent("authenticated", ev);
        return std::make_pair(std::move(w), ok);
    };
    JsonValue ev;

    std::cout << "1. sign-in required\n";
    for (const char* path : {"/api/chats/direct", "/api/groups", "/api/groups/1/leave", "/api/groups/1/mute", "/api/groups/1/read"})
        CHECK(call("POST", path, "{\"user_id\":1}", "") && r.status == 401);
    CHECK(call("GET", "/api/users", "", "") && r.status == 401);

    std::cout << "2. the people directory and online status\n";
    CHECK(call("GET", "/api/users", "", tA) && r.status == 200 && r.json()["users"].size() == 3);
    CHECK(findIn(r.json()["users"], "user_id", idA).isNull());                                   // never lists yourself
    CHECK(!findIn(r.json()["users"], "user_id", idB)["online"].asBool());                        // nobody is connected yet
    CHECK(call("GET", "/api/users?q=bal", "", tA) && r.json()["users"].size() == 1 && r.json()["users"].at(0)["name"].asString() == "Bala Nair");
    CHECK(call("GET", "/api/users?q=" + std::string(51, 'x'), "", tA) && r.status == 400);

    auto [wa, okA] = connect(tA);
    CHECK(okA);
    JsonValue snap;
    CHECK(wa->waitEvent("presence_snapshot", snap) && snap["data"]["user_ids"].size() == 1);
    auto [wb, okB] = connect(tB);
    CHECK(okB && waitPresence(*wa, idB, true));                                                    // A hears that B came online
    CHECK(wb->waitEvent("presence_snapshot", snap) && snap["data"]["user_ids"].size() == 2);
    CHECK(call("GET", "/api/users", "", tA) && findIn(r.json()["users"], "user_id", idB)["online"].asBool() && !findIn(r.json()["users"], "user_id", idC)["online"].asBool());
    wb.reset();                                                                                      // B closes the page
    CHECK(waitPresence(*wa, idB, false));
    auto [wb2, okB2] = connect(tB);
    auto [wc, okC] = connect(tC);
    CHECK(okB2 && okC);

    std::cout << "3. direct chats\n";
    CHECK(call("POST", "/api/chats/direct", "{\"user_id\":" + num(idB) + "}", tA) && r.status == 200);
    const auto dm = r.json()["group"];
    const int dmId = dm["group_id"].asInt();
    CHECK(dm["type"].asString() == "PRIVATE" && dm["name"].asString() == "Bala Nair" && dm["other_user_id"].asInt() == idB);
    CHECK(wb2->waitEvent("group_added", ev) && ev["data"]["group_id"].asInt() == dmId && ev["data"]["name"].asString() == "Asha Rao");   // B sees it at once, named for A
    CHECK(call("POST", "/api/chats/direct", "{\"user_id\":" + num(idA) + "}", tB) && r.json()["group"]["group_id"].asInt() == dmId && r.json()["group"]["name"].asString() == "Asha Rao");
    CHECK(call("POST", "/api/chats/direct", "{\"user_id\":" + num(idA) + "}", tA) && r.status == 400);          // yourself
    CHECK(call("POST", "/api/chats/direct", "{\"user_id\":999}", tA) && r.status == 404);
    CHECK(call("POST", "/api/chats/direct", "{\"x\":1}", tA) && r.status == 400);
    CHECK(call("POST", "/api/chats/direct", "{}", tA) && r.status == 400);
    CHECK(call("GET", "/api/groups/" + num(dmId) + "/messages", "", tC) && r.status == 404);                      // a private chat is private
    CHECK(call("POST", "/api/groups/" + num(dmId) + "/messages", "{\"content\":\"hi Bala\"}", tA) && r.status == 201);
    CHECK(call("GET", "/api/groups", "", tB) && findIn(r.json()["groups"], "group_id", dmId)["unread_count"].asInt() == 1);
    CHECK(findIn(r.json()["groups"], "group_id", dmId)["last_message"]["content"].asString() == "hi Bala");
    CHECK(call("GET", "/api/groups", "", tA) && findIn(r.json()["groups"], "group_id", dmId)["unread_count"].asInt() == 0);      // your own message is never unread for you

    std::cout << "4. user-made groups\n";
    CHECK(call("POST", "/api/groups", "{\"name\":\"Study circle\",\"member_ids\":[" + num(idB) + "," + num(idC) + "," + num(idC) + "]}", tA) && r.status == 201);
    const int gid = r.json()["group"]["group_id"].asInt();
    CHECK(r.json()["group"]["type"].asString() == "GROUP" && r.json()["group"]["member_count"].asInt() == 3 && r.json()["group"]["name"].asString() == "Study circle");
    CHECK(wb2->waitEvent("group_added", ev) && ev["data"]["group_id"].asInt() == gid);
    CHECK(wc->waitEvent("group_added", ev) && ev["data"]["group_id"].asInt() == gid);
    CHECK(call("GET", "/api/groups/" + num(gid) + "/messages", "", tD) && r.status == 404);
    CHECK(call("POST", "/api/groups", "{\"name\":\"  \",\"member_ids\":[2]}", tA) && r.status == 400);
    CHECK(call("POST", "/api/groups", "{\"name\":\"" + std::string(61, 'x') + "\",\"member_ids\":[2]}", tA) && r.status == 400);
    CHECK(call("POST", "/api/groups", "{\"name\":\"G\",\"member_ids\":[]}", tA) && r.status == 400);
    CHECK(call("POST", "/api/groups", "{\"name\":\"G\",\"member_ids\":[" + num(idA) + "]}", tA) && r.status == 400);   // only yourself
    CHECK(call("POST", "/api/groups", "{\"name\":\"G\",\"member_ids\":[999]}", tA) && r.status == 404);
    CHECK(call("POST", "/api/groups", "{\"name\":\"G\",\"member_ids\":\"2\"}", tA) && r.status == 400);
    {
        std::string many = "[";
        for (int i = 1; i <= 50; ++i) many += (i > 1 ? "," : "") + num(i);
        CHECK(call("POST", "/api/groups", "{\"name\":\"G\",\"member_ids\":" + many + "]}", tA) && r.status == 400);
    }
    CHECK(call("POST", "/api/groups/" + num(gid) + "/leave", "{}", tB) && r.status == 200);
    CHECK(call("GET", "/api/groups", "", tB) && findIn(r.json()["groups"], "group_id", gid).isNull());
    CHECK(call("GET", "/api/groups", "", tA) && findIn(r.json()["groups"], "group_id", gid)["member_count"].asInt() == 2);
    CHECK(call("POST", "/api/groups/1/leave", "{}", tA) && r.status == 400);                                       // course groups cannot be left
    CHECK(call("POST", "/api/groups/" + num(dmId) + "/leave", "{}", tA) && r.status == 400);                       // nor private chats
    CHECK(call("POST", "/api/groups/" + num(gid) + "/leave", "{}", tD) && r.status == 404);
    CHECK(call("POST", "/api/groups/abc/leave", "{}", tA) && r.status == 400);

    std::cout << "5. mute is personal\n";
    CHECK(call("POST", "/api/groups/" + num(gid) + "/mute", "{\"muted\":true}", tA) && r.status == 200 && r.json()["muted"].asBool());
    CHECK(call("GET", "/api/groups", "", tA) && findIn(r.json()["groups"], "group_id", gid)["muted"].asBool());
    CHECK(call("GET", "/api/groups", "", tC) && !findIn(r.json()["groups"], "group_id", gid)["muted"].asBool());
    CHECK(call("POST", "/api/groups/" + num(gid) + "/mute", "{\"x\":1}", tA) && r.status == 400);
    CHECK(call("POST", "/api/groups/" + num(gid) + "/mute", "{\"muted\":true}", tD) && r.status == 404);
    CHECK(call("POST", "/api/groups/" + num(gid) + "/mute", "{\"muted\":false}", tA) && !r.json()["muted"].asBool());

    std::cout << "6. unread counts and marking read\n";
    CHECK(call("POST", "/api/groups/" + num(gid) + "/messages", "{\"content\":\"one\"}", tC) && r.status == 201);
    CHECK(call("POST", "/api/groups/" + num(gid) + "/messages", "{\"content\":\"second\"}", tC) && r.status == 201);
    CHECK(call("GET", "/api/groups", "", tA) && findIn(r.json()["groups"], "group_id", gid)["unread_count"].asInt() == 2);
    CHECK(findIn(r.json()["groups"], "group_id", gid)["last_message"]["content"].asString() == "second");
    CHECK(r.json()["groups"].at(0)["group_id"].asInt() == gid);                                                    // newest activity first
    CHECK(call("GET", "/api/groups", "", tC) && findIn(r.json()["groups"], "group_id", gid)["unread_count"].asInt() == 0);
    CHECK(call("POST", "/api/groups/" + num(gid) + "/read", "", tA) && r.status == 200 && r.json()["last_read_message_id"].asInt() > 0);
    CHECK(call("GET", "/api/groups", "", tA) && findIn(r.json()["groups"], "group_id", gid)["unread_count"].asInt() == 0);
    CHECK(call("POST", "/api/groups/" + num(gid) + "/messages", "{\"content\":\"third\"}", tC) && r.status == 201);
    const int thirdId = r.json()["message"]["message_id"].asInt();
    CHECK(call("GET", "/api/groups", "", tA) && findIn(r.json()["groups"], "group_id", gid)["unread_count"].asInt() == 1);
    CHECK(call("POST", "/api/groups/" + num(gid) + "/read", "{\"message_id\":" + num(thirdId) + "}", tA) && r.json()["last_read_message_id"].asInt() == thirdId);
    CHECK(call("POST", "/api/groups/" + num(gid) + "/read", "{\"message_id\":\"x\"}", tA) && r.status == 400);
    CHECK(call("POST", "/api/groups/" + num(gid) + "/read", "{}", tD) && r.status == 404);
    CHECK(call("POST", "/api/groups/" + num(dmId) + "/messages", "{\"content\":\"later\"}", tA) && r.status == 201);
    CHECK(call("GET", "/api/groups", "", tA) && r.json()["groups"].at(0)["group_id"].asInt() == dmId);            // a newer message moves its chat to the top

    std::cout << "7. typing indicator\n";
    CHECK(wa->sendText(subFrame(gid)) && wa->waitEvent("subscribed", ev));
    CHECK(wc->sendText(subFrame(gid)) && wc->waitEvent("subscribed", ev));
    CHECK(wc->sendText("{\"action\":\"typing\",\"group_id\":" + num(gid) + ",\"typing\":true}"));
    CHECK(wa->waitEvent("typing", ev) && ev["data"]["user_id"].asInt() == idC && ev["data"]["name"].asString() == "Chitra Das" && ev["data"]["typing"].asBool() && ev["data"]["group_id"].asInt() == gid);
    CHECK(!wc->waitEvent("typing", ev, 400));                                                                       // the typist never hears their own typing
    CHECK(wc->sendText("{\"action\":\"typing\",\"group_id\":" + num(gid) + ",\"typing\":false}") && wa->waitEvent("typing", ev) && !ev["data"]["typing"].asBool());
    auto [wd, okD] = connect(tD);
    CHECK(okD && wd->sendText("{\"action\":\"typing\",\"group_id\":" + num(gid) + "}") && wd->waitEvent("error", ev) && ev["code"].asInt() == 404);   // not in the group
    {
        TestWsClient anon(PORT);
        CHECK(anon.handshake() && anon.sendText("{\"action\":\"typing\",\"group_id\":1}") && anon.waitEvent("error", ev) && ev["code"].asInt() == 401);
    }

    srv.stop();
    th.join();
    WSACleanup();
    if (failures == 0) std::cout << "\nSTAGE 10 OK\n";
    else std::cout << "\nSTAGE 10 FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
