#include "http/HttpResponse.h"
#include <sstream>

namespace uniconnect {
namespace http {

HttpResponse::HttpResponse() : status_code(200), status_text("OK") {
    setHeader("Server", "UniConnect-C++20-OS-Backend");
    setHeader("Access-Control-Allow-Origin", "*");
    setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
}

HttpResponse::HttpResponse(int code, std::string text)
    : status_code(code), status_text(std::move(text)) {
    setHeader("Server", "UniConnect-C++20-OS-Backend");
    setHeader("Access-Control-Allow-Origin", "*");
    setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
}

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    headers[key] = value;
}

void HttpResponse::setBody(std::string b, const std::string& content_type) {
    body = std::move(b);
    setHeader("Content-Type", content_type);
    setHeader("Content-Length", std::to_string(body.size()));
}

void HttpResponse::setJson(const utils::JsonValue& json_val) {
    setBody(json_val.serialize(false), "application/json");
}

std::string HttpResponse::toString() const {
    std::ostringstream oss;
    oss << version << " " << status_code << " " << status_text << "\r\n";
    for (const auto& [k, v] : headers) {
        oss << k << ": " << v << "\r\n";
    }
    oss << "\r\n";
    oss << body;
    return oss.str();
}

HttpResponse HttpResponse::ok(const std::string& body_text) {
    HttpResponse resp(200, "OK");
    resp.setBody(body_text, "text/plain");
    return resp;
}

HttpResponse HttpResponse::okJson(const utils::JsonValue& json_val) {
    HttpResponse resp(200, "OK");
    resp.setJson(json_val);
    return resp;
}

HttpResponse HttpResponse::created(const utils::JsonValue& json_val) {
    HttpResponse resp(201, "Created");
    resp.setJson(json_val);
    return resp;
}

HttpResponse HttpResponse::badRequest(const std::string& message) {
    HttpResponse resp(400, "Bad Request");
    auto err = utils::JsonValue::object();
    err["error"] = message;
    resp.setJson(err);
    return resp;
}

HttpResponse HttpResponse::unauthorized(const std::string& message) {
    HttpResponse resp(401, "Unauthorized");
    auto err = utils::JsonValue::object();
    err["error"] = message;
    resp.setJson(err);
    return resp;
}

HttpResponse HttpResponse::forbidden(const std::string& message) {
    HttpResponse resp(403, "Forbidden");
    auto err = utils::JsonValue::object();
    err["error"] = message;
    resp.setJson(err);
    return resp;
}

HttpResponse HttpResponse::notFound(const std::string& message) {
    HttpResponse resp(404, "Not Found");
    auto err = utils::JsonValue::object();
    err["error"] = message;
    resp.setJson(err);
    return resp;
}

HttpResponse HttpResponse::serverError(const std::string& message) {
    HttpResponse resp(500, "Internal Server Error");
    auto err = utils::JsonValue::object();
    err["error"] = message;
    resp.setJson(err);
    return resp;
}

HttpResponse HttpResponse::corsOptions() {
    HttpResponse resp(204, "No Content");
    resp.setHeader("Content-Length", "0");
    return resp;
}

HttpResponse HttpResponse::webSocketHandshake(const std::string& accept_key) {
    HttpResponse resp(101, "Switching Protocols");
    resp.setHeader("Upgrade", "websocket");
    resp.setHeader("Connection", "Upgrade");
    resp.setHeader("Sec-WebSocket-Accept", accept_key);
    return resp;
}

} // namespace http
} // namespace uniconnect
