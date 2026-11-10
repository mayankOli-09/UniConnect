#include "http/HttpParser.h"
#include "http/HttpResponse.h"
#include "websocket/WebSocketParser.h"
#include "websocket/WebSocket.h"
#include "utils/Crypto.h"
#include "utils/Json.h"
#include <iostream>
#include <cassert>

using namespace uniconnect;

void testHttpParser() {
    std::cout << "[TestServer] Testing HTTP Request Parser..." << std::endl;
    std::string raw_http = 
        "POST /api/enroll?term=fall HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 43\r\n"
        "\r\n"
        "{\"student_id\":\"std-1\",\"section_id\":\"sec-1\"}";

    http::HttpRequest req;
    size_t consumed = 0;
    auto res = http::HttpParser::parse(raw_http, req, consumed);

    assert(res == http::HttpParseResult::COMPLETE);
    assert(req.method == http::HttpMethod::POST);
    assert(req.path == "/api/enroll");
    assert(req.getQueryParam("term") == "fall");
    assert(req.getHeader("Content-Type") == "application/json");

    auto json = req.getJsonBody();
    assert(json["student_id"].asString() == "std-1");
    assert(json["section_id"].asString() == "sec-1");
    std::cout << " -> PASSED" << std::endl;
}

void testWebSocketRFC6455Handshake() {
    std::cout << "[TestServer] Testing RFC 6455 WebSocket Handshake..." << std::endl;
    // Standard RFC 6455 test vector
    std::string client_nonce = "dGhlIHNhbXBsZSBub25jZQ==";
    std::string expected_accept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

    std::string generated_accept = utils::Crypto::generateWebSocketAcceptKey(client_nonce);
    assert(generated_accept == expected_accept);
    std::cout << " -> PASSED: RFC 6455 Sec-WebSocket-Accept matched standard test vector!" << std::endl;
}

void testWebSocketFrames() {
    std::cout << "[TestServer] Testing WebSocket Frame Encoding & Decoding..." << std::endl;
    std::string message = "UniConnect Real-Time WebSocket Message";

    // 1. Server encoding (unmasked per RFC 6455)
    std::string server_frame = websocket::WebSocketParser::encodeFrame(message, websocket::WsOpCode::TEXT, true);
    assert(!server_frame.empty());
    assert(static_cast<uint8_t>(server_frame[0]) == 0x81); // FIN (0x80) | TEXT (0x01)

    // 2. Decode the frame
    websocket::WsFrame decoded;
    size_t consumed = 0;
    auto status = websocket::WebSocketParser::parseFrame(
        reinterpret_cast<const uint8_t*>(server_frame.data()),
        server_frame.size(),
        decoded,
        consumed
    );

    assert(status == websocket::WsParseStatus::FRAME_READY);
    assert(decoded.opcode == websocket::WsOpCode::TEXT);
    assert(decoded.payload == message);
    assert(consumed == server_frame.size());
    std::cout << " -> PASSED" << std::endl;
}

void testJson() {
    std::cout << "[TestServer] Testing JSON Parsing and Serializing..." << std::endl;
    std::string raw_json = R"({"name":"Operating Systems","credits":4,"active":true,"tags":["concurrency","epoll"]})";
    utils::JsonValue val = utils::JsonValue::parse(raw_json);

    assert(val["name"].asString() == "Operating Systems");
    assert(val["credits"].asInt() == 4);
    assert(val["active"].asBool() == true);
    assert(val["tags"].isArray());
    assert(val["tags"][0].asString() == "concurrency");
    assert(val["tags"][1].asString() == "epoll");
    std::cout << " -> PASSED" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "      UniConnect Server & WS Tests      " << std::endl;
    std::cout << "========================================" << std::endl;

    testHttpParser();
    testWebSocketRFC6455Handshake();
    testWebSocketFrames();
    testJson();

    std::cout << "All Server & Protocol tests PASSED!" << std::endl;
    return 0;
}
