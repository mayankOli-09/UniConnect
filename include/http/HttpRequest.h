#pragma once

#include <string>
#include <map>
#include "utils/Json.h"

namespace uniconnect {
namespace http {

enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE_METHOD,
    OPTIONS,
    UNKNOWN
};

inline std::string httpMethodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE_METHOD: return "DELETE";
        case HttpMethod::OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

inline HttpMethod stringToHttpMethod(const std::string& str) {
    if (str == "GET") return HttpMethod::GET;
    if (str == "POST") return HttpMethod::POST;
    if (str == "PUT") return HttpMethod::PUT;
    if (str == "DELETE") return HttpMethod::DELETE_METHOD;
    if (str == "OPTIONS") return HttpMethod::OPTIONS;
    return HttpMethod::UNKNOWN;
}

struct HttpRequest {
    HttpMethod method{HttpMethod::GET};
    std::string path;
    std::string raw_path;
    std::string query_string;
    std::string version{"HTTP/1.1"};
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;
    std::string body;

    std::string getHeader(const std::string& name) const;
    bool hasHeader(const std::string& name) const;
    std::string getQueryParam(const std::string& key, const std::string& default_val = "") const;

    bool isWebSocketUpgrade() const;
    std::string getWebSocketKey() const;

    utils::JsonValue getJsonBody() const;
};

} // namespace http
} // namespace uniconnect
