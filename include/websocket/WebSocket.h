#pragma once
// WebSocket frame format (RFC 6455). The session manager / handshake is added in a later stage.
#include <cstddef>
#include <cstdint>
#include <string>

#include "utils/Crypto.h"

namespace uniconnect {
namespace websocket {

// RFC 6455 handshake: the server proves it understood the upgrade by hashing the client's key with a fixed GUID.
inline std::string computeAcceptKey(const std::string& clientKey) {
    return utils::crypto::base64Encode(utils::crypto::sha1(clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
}

enum class WsOpCode : uint8_t { CONTINUATION = 0x0, TEXT = 0x1, BINARY = 0x2, CLOSE = 0x8, PING = 0x9, PONG = 0xA };

struct WsFrame {
    bool fin = true;
    WsOpCode opcode = WsOpCode::TEXT;
    std::string payload;
};

enum class WsParseStatus { FRAME_READY, NEED_MORE_DATA, INVALID };

class WebSocketParser {
public:
    static constexpr size_t MAX_PAYLOAD_BYTES = 1024 * 1024;

    static WsParseStatus parseFrame(const uint8_t* data, size_t len, WsFrame& out, size_t& consumed) {
        if (len < 2) return WsParseStatus::NEED_MORE_DATA;
        const bool fin = (data[0] & 0x80) != 0;
        const uint8_t op = data[0] & 0x0F;
        const bool masked = (data[1] & 0x80) != 0;
        uint64_t plen = data[1] & 0x7F;
        size_t pos = 2;
        if (plen == 126) {
            if (len < pos + 2) return WsParseStatus::NEED_MORE_DATA;
            plen = (static_cast<uint64_t>(data[2]) << 8) | data[3];
            pos += 2;
        } else if (plen == 127) {
            if (len < pos + 8) return WsParseStatus::NEED_MORE_DATA;
            plen = 0;
            for (int i = 0; i < 8; ++i) plen = (plen << 8) | data[pos + static_cast<size_t>(i)];
            pos += 8;
        }
        if (plen > MAX_PAYLOAD_BYTES) return WsParseStatus::INVALID;
        uint8_t key[4] = {0, 0, 0, 0};
        if (masked) {
            if (len < pos + 4) return WsParseStatus::NEED_MORE_DATA;
            for (int i = 0; i < 4; ++i) key[i] = data[pos + static_cast<size_t>(i)];
            pos += 4;
        }
        if (len < pos + plen) return WsParseStatus::NEED_MORE_DATA;
        out.fin = fin;
        out.opcode = static_cast<WsOpCode>(op);
        out.payload.resize(static_cast<size_t>(plen));
        for (size_t i = 0; i < plen; ++i)
            out.payload[i] = static_cast<char>(masked ? (data[pos + i] ^ key[i % 4]) : data[pos + i]);
        consumed = pos + static_cast<size_t>(plen);
        return WsParseStatus::FRAME_READY;
    }

    // Server -> client frames are never masked.
    static std::string createFrame(WsOpCode op, const std::string& payload) {
        std::string f;
        f += static_cast<char>(0x80 | static_cast<uint8_t>(op));
        const size_t n = payload.size();
        if (n < 126) {
            f += static_cast<char>(n);
        } else if (n <= 0xFFFF) {
            f += static_cast<char>(126);
            f += static_cast<char>((n >> 8) & 0xFF);
            f += static_cast<char>(n & 0xFF);
        } else {
            f += static_cast<char>(127);
            for (int i = 7; i >= 0; --i) f += static_cast<char>((static_cast<uint64_t>(n) >> (8 * i)) & 0xFF);
        }
        f += payload;
        return f;
    }
    static std::string createTextFrame(const std::string& text) { return createFrame(WsOpCode::TEXT, text); }
    static std::string createPongFrame(const std::string& payload) { return createFrame(WsOpCode::PONG, payload); }
    static std::string createCloseFrame() { return createFrame(WsOpCode::CLOSE, ""); }
};

} // namespace websocket
} // namespace uniconnect
