// Stage 2 check: HTTP parsing + Windows event loop.
//   stage2_test.exe          -> runs the automated self-test
//   stage2_test.exe serve    -> keeps a demo server running on port 8080 (try http://localhost:8080/health)
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "http/HttpParser.h"
#include "server/EpollServer.h"
#include "utils/Json.h"
#include "utils/Logger.h"

using namespace uniconnect;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

// ---------------------------------------------------------------- tiny blocking HTTP client
struct Resp { int status = 0; std::map<std::string, std::string> headers; std::string body; };

static SOCKET connectTo(int port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    DWORD timeout = 3000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    if (connect(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) { closesocket(s); return INVALID_SOCKET; }
    return s;
}

static bool sendAll(SOCKET s, const std::string& d) {
    size_t t = 0;
    while (t < d.size()) {
        int n = send(s, d.data() + t, static_cast<int>(d.size() - t), 0);
        if (n <= 0) return false;
        t += static_cast<size_t>(n);
    }
    return true;
}

// Reads exactly one response out of `pending` (refilling from the socket as needed).
static bool readResponse(SOCKET s, std::string& pending, Resp& out) {
    out = Resp{};
    while (true) {
        size_t he = pending.find("\r\n\r\n");
        if (he != std::string::npos) {
            std::string head = pending.substr(0, he);
            size_t le = head.find("\r\n");
            std::string status = head.substr(0, le);
            out.status = std::atoi(status.substr(status.find(' ') + 1).c_str());
            size_t pos = le == std::string::npos ? head.size() : le + 2;
            while (pos < head.size()) {
                size_t e = head.find("\r\n", pos);
                if (e == std::string::npos) e = head.size();
                std::string line = head.substr(pos, e - pos);
                size_t c = line.find(':');
                if (c != std::string::npos) {
                    std::string v = line.substr(c + 1);
                    while (!v.empty() && v[0] == ' ') v.erase(0, 1);
                    out.headers[http::toLowerAscii(line.substr(0, c))] = v;
                }
                pos = e + 2;
            }
            size_t len = out.headers.count("content-length") ? std::stoul(out.headers["content-length"]) : 0;
            if (pending.size() >= he + 4 + len) {
                out.body = pending.substr(he + 4, len);
                pending.erase(0, he + 4 + len);
                return true;
            }
        }
        char buf[4096];
        int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        pending.append(buf, static_cast<size_t>(n));
    }
}

static std::string get(const std::string& path) {
    return "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
}

// ---------------------------------------------------------------- server under test
static void installDemoHandlers(server::EpollServer& srv) {
    srv.setHttpHandler([&srv](int fd, const http::HttpRequest& req) {
        http::HttpResponse res;
        if (req.method == http::HttpMethod::OPTIONS) {
            res = http::HttpResponse::corsOptions();
        } else if (req.path == "/health" || req.path == "/") {
            auto o = utils::JsonValue::object();
            o["status"] = "ok";
            o["service"] = "UniConnect Backend";
            res = http::HttpResponse::okJson(o);
        } else if (req.path == "/echo" && req.method == http::HttpMethod::POST) {
            auto j = req.getJsonBody();
            j["received"] = true;
            res = http::HttpResponse::okJson(j);
        } else if (req.path == "/q") {
            auto o = utils::JsonValue::object();
            o["user_id"] = req.getQueryParam("user_id");
            res = http::HttpResponse::okJson(o);
        } else if (req.path == "/boom") {
            throw std::runtime_error("deliberate test error");
        } else {
            res = http::HttpResponse::notFound("Endpoint not found: " + req.path);
        }
        srv.sendData(fd, res.toString());
    });
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "serve") {
        server::EpollServer srv(8080);
        installDemoHandlers(srv);
        std::cout << "Demo server: http://localhost:8080/health   (Ctrl+C to stop)\n";
        try { srv.start(); } catch (const std::exception& e) { std::cout << "ERROR: " << e.what() << "\n"; return 1; }
        return 0;
    }

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    const int PORT = 18080;

    server::EpollServer srv(PORT);
    installDemoHandlers(srv);
    std::string startError;
    std::thread th([&] { try { srv.start(); } catch (const std::exception& e) { startError = e.what(); } });
    for (int i = 0; i < 200 && !srv.isRunning() && startError.empty(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!startError.empty() || !srv.isRunning()) {
        std::cout << "Could not start server: " << startError << "\n";
        srv.stop(); th.join();
        return 1;
    }

    Resp r;
    std::string pend;

    std::cout << "1. GET /health\n";
    SOCKET s = connectTo(PORT);
    CHECK(s != INVALID_SOCKET);
    CHECK(sendAll(s, get("/health")) && readResponse(s, pend, r));
    CHECK(r.status == 200);
    CHECK(r.headers["content-type"].find("application/json") == 0);
    CHECK(r.headers["access-control-allow-origin"] == "*");
    utils::JsonValue j;
    CHECK(utils::JsonValue::tryParse(r.body, j) && j["status"].asString() == "ok");
    std::cout << "   " << r.body << "\n";

    std::cout << "2. keep-alive: second request on the same connection\n";
    CHECK(sendAll(s, get("/")) && readResponse(s, pend, r) && r.status == 200);

    std::cout << "3. 404 for unknown route\n";
    CHECK(sendAll(s, get("/nope")) && readResponse(s, pend, r) && r.status == 404);
    CHECK(utils::JsonValue::tryParse(r.body, j) && !j["error"].asString().empty());

    std::cout << "4. CORS preflight (OPTIONS)\n";
    CHECK(sendAll(s, "OPTIONS /api/auth/login HTTP/1.1\r\nHost: x\r\nOrigin: http://localhost:5173\r\n\r\n") && readResponse(s, pend, r));
    CHECK(r.status == 204);
    CHECK(r.headers["access-control-allow-headers"].find("Authorization") != std::string::npos);

    std::cout << "5. POST with JSON body\n";
    const std::string body = "{\"email\":\"a@b.com\",\"n\":3}";
    CHECK(sendAll(s, "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: " +
                         std::to_string(body.size()) + "\r\n\r\n" + body) && readResponse(s, pend, r));
    CHECK(r.status == 200);
    CHECK(utils::JsonValue::tryParse(r.body, j) && j["email"].asString() == "a@b.com" && j["received"].asBool());

    std::cout << "6. query string decoding\n";
    CHECK(sendAll(s, get("/q?user_id=a%20b%2Bc&x=1")) && readResponse(s, pend, r));
    CHECK(utils::JsonValue::tryParse(r.body, j) && j["user_id"].asString() == "a b+c");

    std::cout << "7. two pipelined requests in one packet\n";
    CHECK(sendAll(s, get("/health") + get("/nope")));
    CHECK(readResponse(s, pend, r) && r.status == 200);
    CHECK(readResponse(s, pend, r) && r.status == 404);

    std::cout << "8. request split across two packets\n";
    CHECK(sendAll(s, "GET /health HTTP/1.1\r\nHo"));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    CHECK(sendAll(s, "st: x\r\n\r\n") && readResponse(s, pend, r) && r.status == 200);

    std::cout << "9. handler exception becomes HTTP 500 (server survives)\n";
    CHECK(sendAll(s, get("/boom")) && readResponse(s, pend, r) && r.status == 500);
    CHECK(sendAll(s, get("/health")) && readResponse(s, pend, r) && r.status == 200);
    closesocket(s);

    std::cout << "10. oversized body is rejected, server survives\n";
    SOCKET s2 = connectTo(PORT);
    std::string pend2;
    CHECK(sendAll(s2, "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: 5000000\r\n\r\n"));
    CHECK(readResponse(s2, pend2, r) && r.status == 400);
    closesocket(s2);
    SOCKET s3 = connectTo(PORT);
    std::string pend3;
    CHECK(sendAll(s3, get("/health")) && readResponse(s3, pend3, r) && r.status == 200);
    closesocket(s3);

    std::cout << "11. garbage request line -> 400\n";
    SOCKET s4 = connectTo(PORT);
    std::string pend4;
    CHECK(sendAll(s4, "this is not http\r\n\r\n") && readResponse(s4, pend4, r) && r.status == 400);
    closesocket(s4);

    std::cout << "12. 20 clients x 25 requests at once\n";
    std::atomic<int> ok{0};
    std::vector<std::thread> clients;
    for (int c = 0; c < 20; ++c) {
        clients.emplace_back([&] {
            SOCKET cs = connectTo(PORT);
            if (cs == INVALID_SOCKET) return;
            std::string p;
            Resp rr;
            for (int k = 0; k < 25; ++k)
                if (sendAll(cs, get("/health")) && readResponse(cs, p, rr) && rr.status == 200) ++ok;
            closesocket(cs);
        });
    }
    for (auto& t : clients) t.join();
    std::cout << "   " << ok << "/500 succeeded\n";
    CHECK(ok == 500);

    std::cout << "13. connections are cleaned up after clients disconnect\n";
    size_t active = 99;
    for (int i = 0; i < 100 && active != 0; ++i) {
        active = srv.getActiveConnectionsCount();
        if (active) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CHECK(active == 0);

    srv.stop();
    th.join();
    WSACleanup();

    if (failures == 0) std::cout << "\nSTAGE 2 OK\n";
    else std::cout << "\nSTAGE 2 FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
