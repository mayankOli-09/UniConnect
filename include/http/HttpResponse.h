#pragma once

#include <string>
#include <map>
#include "utils/Json.h"

namespace uniconnect {
namespace http {

class HttpResponse {
public:
    int status_code{200};
    std::string status_text{"OK"};
    std::string version{"HTTP/1.1"};
    std::map<std::string, std::string> headers;
    std::string body;

    HttpResponse();
    explicit HttpResponse(int code, std::string text = "OK");

    void setHeader(const std::string& key, const std::string& value);
    void setBody(std::string b, const std::string& content_type = "text/plain");
    void setJson(const utils::JsonValue& json_val);

    std::string toString() const;

    // Convenience responses
    static HttpResponse ok(const std::string& body_text = "");
    static HttpResponse okJson(const utils::JsonValue& json_val);
    static HttpResponse created(const utils::JsonValue& json_val);
    static HttpResponse badRequest(const std::string& message = "Bad Request");
    static HttpResponse unauthorized(const std::string& message = "Unauthorized");
    static HttpResponse forbidden(const std::string& message = "Forbidden");
    static HttpResponse notFound(const std::string& message = "Not Found");
    static HttpResponse serverError(const std::string& message = "Internal Server Error");
    static HttpResponse corsOptions();
    static HttpResponse webSocketHandshake(const std::string& accept_key);
};

} // namespace http
} // namespace uniconnect
