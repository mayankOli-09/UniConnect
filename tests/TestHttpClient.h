#pragma once
// Minimal blocking HTTP client for the tests (Winsock).
#include <winsock2.h>
#include <ws2tcpip.h>

#include <map>
#include <string>

#include "http/HttpParser.h"
#include "utils/Json.h"

struct TestResponse {
    int status = 0;
    std::map<std::string, std::string> headers;   // lower-case names
    std::string body;
    uniconnect::utils::JsonValue json() const {
        uniconnect::utils::JsonValue v;
        uniconnect::utils::JsonValue::tryParse(body, v);
        return v;
    }
};

class TestHttpClient {
public:
    explicit TestHttpClient(int port) {
        sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(static_cast<u_short>(port));
        inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
        DWORD timeout = 5000;
        setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        if (connect(sock_, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) { closesocket(sock_); sock_ = INVALID_SOCKET; }
    }
    ~TestHttpClient() { if (sock_ != INVALID_SOCKET) closesocket(sock_); }
    bool connected() const { return sock_ != INVALID_SOCKET; }

    bool sendRaw(const std::string& d) {
        size_t t = 0;
        while (t < d.size()) {
            const int n = send(sock_, d.data() + t, static_cast<int>(d.size() - t), 0);
            if (n <= 0) return false;
            t += static_cast<size_t>(n);
        }
        return true;
    }

    bool request(const std::string& method, const std::string& path, const std::string& body, TestResponse& out,
                 const std::string& extraHeaders = "") {
        std::string req = method + " " + path + " HTTP/1.1\r\nHost: localhost\r\n" + extraHeaders;
        if (!body.empty() || method == "POST") req += "Content-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n";
        req += "\r\n" + body;
        return sendRaw(req) && readResponse(out);
    }
    bool get(const std::string& path, TestResponse& out) { return request("GET", path, "", out); }
    bool post(const std::string& path, const std::string& body, TestResponse& out) { return request("POST", path, body, out); }

    bool readResponse(TestResponse& out) {
        out = TestResponse{};
        while (true) {
            const size_t he = pending_.find("\r\n\r\n");
            if (he != std::string::npos) {
                const std::string head = pending_.substr(0, he);
                size_t le = head.find("\r\n");
                const std::string statusLine = head.substr(0, le);
                out.status = std::atoi(statusLine.substr(statusLine.find(' ') + 1).c_str());
                size_t pos = le == std::string::npos ? head.size() : le + 2;
                while (pos < head.size()) {
                    size_t e = head.find("\r\n", pos);
                    if (e == std::string::npos) e = head.size();
                    const std::string line = head.substr(pos, e - pos);
                    const size_t c = line.find(':');
                    if (c != std::string::npos) {
                        std::string v = line.substr(c + 1);
                        while (!v.empty() && v[0] == ' ') v.erase(0, 1);
                        out.headers[uniconnect::http::toLowerAscii(line.substr(0, c))] = v;
                    }
                    pos = e + 2;
                }
                const size_t len = out.headers.count("content-length") ? std::stoul(out.headers["content-length"]) : 0;
                const bool bodyless = (out.status == 204 || out.status == 101);
                const size_t need = bodyless ? 0 : len;
                if (pending_.size() >= he + 4 + need) {
                    out.body = pending_.substr(he + 4, need);
                    pending_.erase(0, he + 4 + need);
                    return true;
                }
            }
            char buf[4096];
            const int n = recv(sock_, buf, sizeof(buf), 0);
            if (n <= 0) return false;
            pending_.append(buf, static_cast<size_t>(n));
        }
    }

private:
    SOCKET sock_ = INVALID_SOCKET;
    std::string pending_;
};
