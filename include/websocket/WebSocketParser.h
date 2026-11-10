#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace uniconnect {
namespace websocket {

enum class WsOpCode : uint8_t {
    CONTINUATION = 0x0,
    TEXT         = 0x1,
    BINARY       = 0x2,
    CLOSE        = 0x8,
    PING         = 0x9,
    PONG         = 0xA
};

struct WsFrame {
    bool fin{true};
    WsOpCode opcode{WsOpCode::TEXT};
    bool is_masked{false};
    uint8_t mask_key[4]{0, 0, 0, 0};
    std::string payload;

    bool isControl() const {
        return (static_cast<uint8_t>(opcode) & 0x8) != 0;
    }
};

enum class WsParseStatus {
    NEED_MORE_DATA,
    FRAME_READY,
    PROTOCOL_ERROR
};

class WebSocketParser {
public:
    // Parse an incoming frame from buffer; updates bytes_consumed
    static WsParseStatus parseFrame(const uint8_t* data, size_t len, WsFrame& out_frame, size_t& bytes_consumed);

    // Encode a text or binary frame for server -> client transmission (RFC 6455 unmasked)
    static std::string encodeFrame(const std::string& payload, WsOpCode opcode = WsOpCode::TEXT, bool fin = true);

    // Helper control frames
    static std::string createPingFrame(const std::string& data = "");
    static std::string createPongFrame(const std::string& data = "");
    static std::string createCloseFrame(uint16_t code = 1000, const std::string& reason = "");
};

} // namespace websocket
} // namespace uniconnect
