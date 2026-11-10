#include "http/HttpRequest.h"
#include <algorithm>

namespace uniconnect {
namespace http {

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return s;
}

std::string HttpRequest::getHeader(const std::string& name) const {
    std::string lower_name = toLower(name);
    for (const auto& [k, v] : headers) {
        if (toLower(k) == lower_name) {
            return v;
        }
    }
    return "";
}

bool HttpRequest::hasHeader(const std::string& name) const {
    std::string lower_name = toLower(name);
    for (const auto& [k, v] : headers) {
        if (toLower(k) == lower_name) {
            return true;
        }
    }
    return false;
}

std::string HttpRequest::getQueryParam(const std::string& key, const std::string& default_val) const {
    auto it = query_params.find(key);
    return it != query_params.end() ? it->second : default_val;
}

bool HttpRequest::isWebSocketUpgrade() const {
    std::string upgrade = toLower(getHeader("Upgrade"));
    std::string conn = toLower(getHeader("Connection"));
    return (upgrade == "websocket" && conn.find("upgrade") != std::string::npos);
}

std::string HttpRequest::getWebSocketKey() const {
    return getHeader("Sec-WebSocket-Key");
}

utils::JsonValue HttpRequest::getJsonBody() const {
    if (body.empty()) {
        return utils::JsonValue::object();
    }
    utils::JsonValue val;
    if (utils::JsonValue::tryParse(body, val)) {
        return val;
    }
    return utils::JsonValue::object();
}

} // namespace http
} // namespace uniconnect
