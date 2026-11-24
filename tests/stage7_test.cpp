// Stage 7 check: real-time WebSocket chat, delivered through the scheduler and thread pool.
#include <winsock2.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "TestHttpClient.h"
#include "TestWsClient.h"
#include "database/InMemoryChatRepository.h"
#include "database/InMemoryUserRepository.h"
#include "server/Server.h"
#include "utils/Crypto.h"

using namespace uniconnect;
using utils::JsonValue;
using websocket::WsFrame;
using websocket::WsOpCode;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

static std::string q(const std::string& s) { return "\"" + s + "\""; }
static void pause(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    const int PORT = 18084;

    std::cout << "1. SHA-1 and the RFC 6455 handshake key\n";
    CHECK(utils::crypto::toHex(utils::crypto::sha1("abc")) == "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(utils::crypto::toHex(utils::crypto::sha1("")) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(utils::crypto::toHex(utils::crypto::sha1(std::string(1000, 'a'))) == "291e9a6c66994949b57ba5e650361e98fc36b1ba");
    CHECK(websocket::computeAcceptKey("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");   // the example from the RFC
    CHECK(utils::crypto::base64Encode("Man") == "TWFu" && utils::crypto::base64Encode("Ma") == "TWE=" && utils::crypto::base64Encode("M") == "TQ==");

    database::InMemoryUserRepository users;
    database::InMemoryChatRepository chat;
    services::AuthConfig cfg;
    cfg.jwt_secret = "stage7-test-secret-0123456789";
    server::ServerConfig sc;
    sc.port = PORT;
    sc.worker_threads = 3;
    server::Server srv(sc, server::ServerDeps{&users, nullptr, &chat}, cfg);
    std::string startError;
    std::thread th([&] { try { srv.start(); } catch (const std::exception& e) { startError = e.what(); } });
    for (int i = 0; i < 300 && !srv.isRunning() && startError.empty(); ++i) pause(10);
    if (!srv.isRunning()) { std::cout << "Could not start server: " << startError << "\n"; srv.stop(); th.join(); return 1; }

    TestHttpClient http(PORT);
    TestResponse r;
    auto signUp = [&](const std::string& name, const std::string& email) {
        http.post("/api/auth/register", "{" + q("name") + ":" + q(name) + "," + q("email") + ":" + q(email) + "," + q("password") + ":" + q("Password@123") + "," + q("department_id") + ":" + q("1") + "}", r);
        return std::make_pair(r.json()["token"].asString(), r.json()["user"]["user_id"].asInt());
    };
    const auto [tA, idA] = signUp("Asha Rao", "asha@example.com");
    const auto [tB, idB] = signUp("Bala Nair", "bala@example.com");
    const auto [tC, idC] = signUp("Chitra Das", "chitra@example.com");
    const auto [tD, idD] = signUp("Dev Iyer", "dev@example.com");
    chat.addGroup(1, "OS - Section A", "COURSE", {idA, idB, idD});
    chat.addGroup(2, "CSE Department", "DEPARTMENT", {idA, idB, idC, idD});
    chat.setUserName(idA, "Asha Rao");
    chat.setUserName(idB, "Bala Nair");
    chat.setUserName(idD, "Dev Iyer");
    auto authFrame = [](const std::string& token) { return "{\"action\":\"authenticate\",\"token\":\"" + token + "\"}"; };
    auto subFrame = [](const std::string& group) { return "{\"action\":\"subscribe\",\"group_id\":\"" + group + "\"}"; };

    std::cout << "2. the upgrade handshake\n";
    {   // the server closes a connection after rejecting an upgrade, so each bad attempt gets its own client
        TestHttpClient h1(PORT);
        CHECK(h1.request("GET", "/ws", "", r, "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: short\r\n") && r.status == 400);
    }
    {
        TestHttpClient h2(PORT);
        CHECK(h2.request("GET", "/ws", "", r, "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 8\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n") && r.status == 426);
    }
    { TestWsClient wrongPath(PORT); CHECK(!wrongPath.handshake("/chat")); }
    CHECK(http.get("/ws", r) && r.status == 404);                                    // plain GET (no Upgrade header) is not a WebSocket
    TestWsClient a(PORT), b(PORT), c(PORT), d(PORT);
    CHECK(a.handshake() && b.handshake() && c.handshake() && d.handshake());
    pause(100);
    CHECK(srv.websocketSessions() == 4);

    std::cout << "3. nothing works before signing in\n";
    JsonValue ev;
    CHECK(a.sendText(subFrame("1")) && a.waitEvent("error", ev) && ev["code"].asInt() == 401);
    CHECK(a.sendText("{\"action\":\"send_message\",\"group_id\":\"1\",\"content\":\"hi\"}") && a.waitEvent("error", ev) && ev["code"].asInt() == 401);
    CHECK(a.sendText(authFrame("not.a.token")) && a.waitEvent("error", ev) && ev["code"].asInt() == 401);
    CHECK(a.sendText("not json") && a.waitEvent("error", ev) && ev["code"].asInt() == 400);

    std::cout << "4. sign in and join groups (only your own)\n";
    CHECK(a.sendText(authFrame(tA)) && a.waitEvent("authenticated", ev) && ev["user_id"].asInt() == idA);
    CHECK(b.sendText(authFrame(tB)) && b.waitEvent("authenticated", ev));
    CHECK(c.sendText(authFrame(tC)) && c.waitEvent("authenticated", ev));
    CHECK(d.sendText(authFrame(tD)) && d.waitEvent("authenticated", ev));
    CHECK(c.sendText(subFrame("1")) && c.waitEvent("error", ev) && ev["code"].asInt() == 404);       // Chitra is not in group 1
    CHECK(a.sendText(subFrame("99")) && a.waitEvent("error", ev) && ev["code"].asInt() == 404);
    CHECK(a.sendText(subFrame("abc")) && a.waitEvent("error", ev) && ev["code"].asInt() == 400);
    CHECK(a.sendText("{\"action\":\"fly\"}") && a.waitEvent("error", ev) && ev["code"].asInt() == 400);
    CHECK(a.sendText(subFrame("1")) && a.waitEvent("subscribed", ev) && ev["group_id"].asInt() == 1);
    CHECK(b.sendText(subFrame("1")) && b.waitEvent("subscribed", ev));
    CHECK(c.sendText(subFrame("2")) && c.waitEvent("subscribed", ev));
    CHECK(d.sendText(subFrame("2")) && d.waitEvent("subscribed", ev));                              // Dev is in group 1 but only listens to group 2

    std::cout << "5. a message sent over WebSocket reaches the group live, through the scheduler\n";
    const auto doneBefore = srv.dispatcher().scheduler().getMetrics().total_tasks_completed;
    CHECK(a.sendText("{\"action\":\"send_message\",\"group_id\":\"1\",\"content\":\"hello team\",\"priority\":7}"));
    CHECK(a.waitEvent("new_message", ev) && ev["data"]["content"].asString() == "hello team" && ev["data"]["sender_name"].asString() == "Asha Rao" && ev["data"]["group_id"].asInt() == 1);
    CHECK(b.waitEvent("new_message", ev) && ev["data"]["content"].asString() == "hello team" && ev["data"]["sender_id"].asInt() == idA);
    CHECK(!d.waitEvent("new_message", ev, 400));                                                     // not subscribed to group 1
    CHECK(!c.waitEvent("new_message", ev, 200));                                                     // not a member
    pause(100);
    CHECK(srv.dispatcher().scheduler().getMetrics().total_tasks_completed > doneBefore);             // it really was a scheduled task
    CHECK(http.get("/health", r) && r.status == 200);

    std::cout << "6. a REST message is pushed over WebSocket too\n";
    CHECK(http.request("POST", "/api/groups/1/messages", "{\"content\":\"from the web page\"}", r, "Authorization: Bearer " + tB + "\r\n") && r.status == 201);
    CHECK(r.json()["message"]["content"].asString() == "from the web page");
    CHECK(a.waitEvent("new_message", ev) && ev["data"]["content"].asString() == "from the web page" && ev["data"]["sender_name"].asString() == "Bala Nair");
    CHECK(b.waitEvent("new_message", ev));

    std::cout << "7. someone removed from the group stops receiving, even with the socket still open\n";
    chat.removeMember(1, idB);
    CHECK(a.sendText("{\"action\":\"send_message\",\"group_id\":\"1\",\"content\":\"after Bala left\"}") && a.waitEvent("new_message", ev) && ev["data"]["content"].asString() == "after Bala left");
    CHECK(!b.waitEvent("new_message", ev, 500));
    CHECK(b.sendText("{\"action\":\"send_message\",\"group_id\":\"1\",\"content\":\"can I still talk?\"}") && b.waitEvent("error", ev) && ev["code"].asInt() == 404);

    std::cout << "8. validation over the socket\n";
    CHECK(a.sendText("{\"action\":\"send_message\",\"group_id\":\"1\",\"content\":\"   \"}") && a.waitEvent("error", ev) && ev["code"].asInt() == 400);
    CHECK(a.sendText("{\"action\":\"send_message\",\"group_id\":\"1\",\"content\":\"x\",\"sender_id\":\"" + std::to_string(idB) + "\"}") && a.waitEvent("error", ev) && ev["code"].asInt() == 403);
    CHECK(a.sendText("{\"action\":\"send_message\",\"group_id\":\"1\",\"content\":\"x\",\"priority\":99}") && a.waitEvent("error", ev) && ev["code"].asInt() == 400);

    std::cout << "9. ping/pong and orderly close\n";
    CHECK(a.sendFrame(WsOpCode::PING, "are you there"));
    {
        WsFrame f;
        CHECK(a.recvFrame(f) && f.opcode == WsOpCode::PONG && f.payload == "are you there");
    }
    const size_t before = srv.websocketSessions();
    CHECK(a.sendFrame(WsOpCode::CLOSE, ""));
    {
        WsFrame f;
        CHECK(a.recvFrame(f) && f.opcode == WsOpCode::CLOSE);
    }
    pause(200);
    CHECK(srv.websocketSessions() == before - 1);

    std::cout << "10. spam limit over the socket (30 per 10 seconds)\n";
    for (int i = 0; i < 40; ++i) d.sendText("{\"action\":\"send_message\",\"group_id\":\"1\",\"content\":\"spam " + std::to_string(i) + "\"}");
    int refused = 0;
    JsonValue e2;
    while (d.waitEvent("error", e2, 600)) if (e2["code"].asInt() == 429) ++refused;
    CHECK(refused == 10);

    std::cout << "11. an oversized frame drops the connection, the server carries on\n";
    {
        TestWsClient big(PORT);
        CHECK(big.handshake());
        big.sendFrame(WsOpCode::TEXT, std::string(2 * 1024 * 1024, 'x'));
        pause(300);
        WsFrame f;
        CHECK(!big.recvFrame(f, 500) && big.closedByPeer());
    }
    CHECK(http.get("/health", r) && r.status == 200);

    std::cout << "12. 25 people in one group all get a live message\n";
    std::vector<std::string> tokens;
    std::vector<int> ids;
    for (int i = 0; i < 25; ++i) { auto [t, id] = signUp("User " + std::to_string(i), "u" + std::to_string(i) + "@example.com"); tokens.push_back(t); ids.push_back(id); }
    chat.addGroup(5, "Everyone", "GROUP", std::set<int>(ids.begin(), ids.end()));
    std::vector<std::unique_ptr<TestWsClient>> crowd;
    int ready = 0;
    for (int i = 0; i < 25; ++i) {
        crowd.push_back(std::make_unique<TestWsClient>(PORT));
        if (crowd.back()->handshake() && crowd.back()->sendText(authFrame(tokens[static_cast<size_t>(i)])) && crowd.back()->waitEvent("authenticated", ev) &&
            crowd.back()->sendText(subFrame("5")) && crowd.back()->waitEvent("subscribed", ev)) ++ready;
    }
    CHECK(ready == 25);
    CHECK(crowd[0]->sendText("{\"action\":\"send_message\",\"group_id\":\"5\",\"content\":\"hello everyone\",\"priority\":10}"));
    int got = 0;
    for (auto& cl : crowd) if (cl->waitEvent("new_message", ev) && ev["data"]["content"].asString() == "hello everyone") ++got;
    CHECK(got == 25);

    std::cout << "13. closing sockets cleans up the sessions\n";
    crowd.clear();
    pause(400);
    CHECK(srv.websocketSessions() <= 3);   // b, c, d remain

    srv.stop();
    th.join();
    WSACleanup();
    if (failures == 0) std::cout << "\nSTAGE 7 OK\n";
    else std::cout << "\nSTAGE 7 FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
