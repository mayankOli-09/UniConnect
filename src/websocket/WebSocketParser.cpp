#include "websocket/WebSocketParser.h"
#include <cstring>
#include <arpa/inet.h>

#if defined(_WIN32)
#include <winsock2.h>
#endif

namespace uniconnect {
namespace websocket {

WsParseStatus WebSocketParser::parseFrame(const uint8_t* data, size_t len, WsFrame& out_frame, size_t& bytes_consumed) {
    bytes_consumed = 0;
    if (len < 2) {
        return WsParseStatus::NEED_MORE_DATA;
    }

    uint8_t byte0 = data[0];
    uint8_t byte1 = data[1];

    out_frame.fin = (byte0 & 0x80) != 0;
    out_frame.opcode = static_cast<WsOpCode>(byte0 & 0x0F);
    out_frame.is_masked = (byte1 & 0x80) != 0;

    uint64_t payload_len = byte1 & 0x7F;
    size_t header_len = 2;

    if (payload_len == 126) {
        if (len < header_len + 2) {
            return WsParseStatus::NEED_MORE_DATA;
        }
        uint16_t ext_len = 0;
        std::memcpy(&ext_len, data + header_len, 2);
        payload_len = ntohs(ext_len);
        header_len += 2;
    } else if (payload_len == 127) {
        if (len < header_len + 8) {
            return WsParseStatus::NEED_MORE_DATA;
        }
        uint64_t ext_len = 0;
        // Big-endian 64-bit unpack
        for (int i = 0; i < 8; ++i) {
            ext_len = (ext_len << 8) | data[header_len + i];
        }
        payload_len = ext_len;
        header_len += 8;
    }

    if (out_frame.is_masked) {
        if (len < header_len + 4) {
            return WsParseStatus::NEED_MORE_DATA;
        }
        std::memcpy(out_frame.mask_key, data + header_len, 4);
        header_len += 4;
    }

    if (len < header_len + payload_len) {
        return WsParseStatus::NEED_MORE_DATA;
    }

    out_frame.payload.resize(payload_len);
    const uint8_t* raw_payload = data + header_len;

    if (out_frame.is_masked) {
        for (size_t i = 0; i < payload_len; ++i) {
            out_frame.payload[i] = static_cast<char>(raw_payload[i] ^ out_frame.mask_key[i % 4]);
        }
    } else {
        std::memcpy(&out_frame.payload[0], raw_payload, payload_len);
    }

    bytes_consumed = header_len + payload_len;
    return WsParseStatus::FRAME_READY;
}

std::string WebSocketParser::encodeFrame(const std::string& payload, WsOpCode opcode, bool fin) {
    std::string frame;
    uint8_t byte0 = (fin ? 0x80 : 0x00) | (static_cast<uint8_t>(opcode) & 0x0F);
    frame.push_back(static_cast<char>(byte0));

    size_t payload_len = payload.size();

    // Server-to-client frames are unmasked (mask bit = 0)
    if (payload_len <= 125) {
        frame.push_back(static_cast<char>(payload_len & 0x7F));
    } else if (payload_len <= 65535) {
        frame.push_back(static_cast<char>(126));
        uint16_t ext_len = htons(static_cast<uint16_t>(payload_len));
        const char* p = reinterpret_cast<const char*>(&ext_len);
        frame.append(p, 2);
    } else {
        frame.push_back(static_cast<char>(127));
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<char>((payload_len >> (i * 8)) & 0xFF));
        }
    }

    frame.append(payload);
    return frame;
}

std::string WebSocketParser::createPingFrame(const std::string& data) {
    return encodeFrame(data, WsOpCode::PING, true);
}

std::string WebSocketParser::createPongFrame(const std::string& data) {
    return encodeFrame(data, WsOpCode::PONG, true);
}

std::string WebSocketParser::createCloseFrame(uint16_t code, const std::string& reason) {
    std::string payload;
    uint16_t net_code = htons(code);
    payload.append(reinterpret_cast<const char*>(&net_code), 2);
    payload.append(reason);
    return encodeFrame(payload, WsOpCode::CLOSE, true);
}

} // namespace websocket
} // namespace uniconnect
