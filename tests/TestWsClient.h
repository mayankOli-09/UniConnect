#pragma once
// Minimal WebSocket client for the tests: does the real HTTP upgrade handshake and sends masked frames, like a browser.
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <string>

#include "utils/Json.h"
#include "websocket/WebSocket.h"

class TestWsClient {
public:
    explicit TestWsClient(int port) {
        sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(static_cast<u_short>(port));
        inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
        if (connect(sock_, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) { closesocket(sock_); sock_ = INVALID_SOCKET; }
    }
    ~TestWsClient() { if (sock_ != INVALID_SOCKET) closesocket(sock_); }
    bool connected() const { return sock_ != INVALID_SOCKET; }
    bool closedByPeer() const { return closed_; }

    // True when the server answered 101 with the correct Sec-WebSocket-Accept.
    bool handshake(const std::string& path = "/ws") {
        const std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
        if (!sendAll("GET " + path + " HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + key +
                     "\r\nSec-WebSocket-Version: 13\r\n\r\n")) return false;
        while (buf_.find("\r\n\r\n") == std::string::npos) if (!fill(3000)) return false;
        const size_t end = buf_.find("\r\n\r\n");
        const std::string head = buf_.substr(0, end);
        buf_.erase(0, end + 4);
        return head.find(" 101 ") != std::string::npos && head.find("Sec-WebSocket-Accept: " + uniconnect::websocket::computeAcceptKey(key)) != std::string::npos;
    }

    bool sendFrame(uniconnect::websocket::WsOpCode op, const std::string& payload) {
        std::string f;
        f += static_cast<char>(0x80 | static_cast<unsigned char>(op));
        const size_t n = payload.size();
        if (n < 126) f += static_cast<char>(0x80 | n);
        else if (n <= 0xFFFF) { f += static_cast<char>(0x80 | 126); f += static_cast<char>(n >> 8); f += static_cast<char>(n & 0xFF); }
        else { f += static_cast<char>(0x80 | 127); for (int i = 7; i >= 0; --i) f += static_cast<char>((static_cast<unsigned long long>(n) >> (8 * i)) & 0xFF); }
        const unsigned char mask[4] = {0x12, 0x34, 0x56, 0x78};   // clients must mask
        f.append(reinterpret_cast<const char*>(mask), 4);
        for (size_t i = 0; i < n; ++i) f += static_cast<char>(payload[i] ^ mask[i % 4]);
        return sendAll(f);
    }
    bool sendText(const std::string& t) { return sendFrame(uniconnect::websocket::WsOpCode::TEXT, t); }
    bool sendAll(const std::string& d) {
        size_t t = 0;
        while (t < d.size()) {
            const int n = send(sock_, d.data() + t, static_cast<int>(d.size() - t), 0);
            if (n <= 0) return false;
            t += static_cast<size_t>(n);
        }
        return true;
    }

    bool recvFrame(uniconnect::websocket::WsFrame& out, int timeoutMs = 3000) {
        while (true) {
            size_t used = 0;
            const auto st = uniconnect::websocket::WebSocketParser::parseFrame(reinterpret_cast<const uint8_t*>(buf_.data()), buf_.size(), out, used);
            if (st == uniconnect::websocket::WsParseStatus::FRAME_READY) { buf_.erase(0, used); return true; }
            if (st == uniconnect::websocket::WsParseStatus::INVALID) return false;
            if (!fill(timeoutMs)) return false;
        }
    }
    // Waits for a JSON event with the given name (other events are skipped).
    bool waitEvent(const std::string& name, uniconnect::utils::JsonValue& out, int timeoutMs = 3000) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            uniconnect::websocket::WsFrame f;
            const int left = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count());
            if (!recvFrame(f, left > 50 ? left : 50)) return false;
            uniconnect::utils::JsonValue j;
            if (f.opcode == uniconnect::websocket::WsOpCode::TEXT && uniconnect::utils::JsonValue::tryParse(f.payload, j) && j["event"].asString() == name) { out = j; return true; }
        }
        return false;
    }

private:
    bool fill(int timeoutMs) {
        DWORD t = static_cast<DWORD>(timeoutMs);
        setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&t), sizeof(t));
        char tmp[4096];
        const int n = recv(sock_, tmp, sizeof(tmp), 0);
        if (n > 0) { buf_.append(tmp, static_cast<size_t>(n)); return true; }
        if (n == 0 || WSAGetLastError() != WSAETIMEDOUT) closed_ = true;
        return false;
    }
    SOCKET sock_ = INVALID_SOCKET;
    std::string buf_;
    bool closed_ = false;
};
