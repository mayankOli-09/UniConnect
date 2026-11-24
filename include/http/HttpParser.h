#pragma once
// HTTP/1.1 request parser + response builder. Header-only, Windows.
// Note: macros such as ERROR and DELETE are defined by <windows.h>, so they are avoided as names here.
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <string>

#include "utils/Json.h"

namespace uniconnect {
namespace http {

enum class HttpMethod { GET, POST, PUT, DELETE_, PATCH, OPTIONS, HEAD, UNKNOWN };

inline std::string toLowerAscii(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::string urlDecode(const std::string& in, bool plusIsSpace) {
    std::string out;
    out.reserve(in.size());
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size() && hex(in[i + 1]) >= 0 && hex(in[i + 2]) >= 0) {
            out += static_cast<char>(hex(in[i + 1]) * 16 + hex(in[i + 2]));
            i += 2;
        } else if (in[i] == '+' && plusIsSpace) {
            out += ' ';
        } else {
            out += in[i];
        }
    }
    return out;
}

struct HttpRequest {
    HttpMethod method = HttpMethod::UNKNOWN;
    std::string method_text;
    std::string target;   // raw request target, e.g. /api/groups?user_id=1
    std::string path;     // decoded path without the query string
    std::string version;  // HTTP/1.1
    std::string body;
    std::map<std::string, std::string> headers;  // lower-case names
    std::map<std::string, std::string> query;    // decoded

    std::string getHeader(const std::string& name, const std::string& def = "") const {
        auto it = headers.find(toLowerAscii(name));
        return it == headers.end() ? def : it->second;
    }
    std::string getQueryParam(const std::string& name, const std::string& def = "") const {
        auto it = query.find(name);
        return it == query.end() ? def : it->second;
    }
    bool isWebSocketUpgrade() const {
        return toLowerAscii(getHeader("upgrade")) == "websocket" &&
               toLowerAscii(getHeader("connection")).find("upgrade") != std::string::npos;
    }
    // Parsed JSON body; an empty object when the body is missing or not valid JSON.
    utils::JsonValue getJsonBody() const {
        utils::JsonValue v;
        if (body.empty() || !utils::JsonValue::tryParse(body, v) || !v.isObject()) return utils::JsonValue::object();
        return v;
    }
};

struct HttpResponse {
    int status_code = 200;
    std::string reason = "OK";
    std::map<std::string, std::string> headers;
    std::string body;

    void setHeader(const std::string& k, const std::string& v) { headers[k] = v; }

    static HttpResponse json(int code, const char* reason, const utils::JsonValue& v) {
        HttpResponse r;
        r.status_code = code;
        r.reason = reason;
        r.headers["Content-Type"] = "application/json; charset=utf-8";
        r.body = v.serialize(false);
        return r;
    }
    static HttpResponse errorJson(int code, const char* reason, const std::string& message) {
        auto o = utils::JsonValue::object();
        o["error"] = message;
        return json(code, reason, o);
    }
    static HttpResponse okJson(const utils::JsonValue& v)  { return json(200, "OK", v); }
    static HttpResponse created(const utils::JsonValue& v) { return json(201, "Created", v); }
    static HttpResponse badRequest(const std::string& m)   { return errorJson(400, "Bad Request", m); }
    static HttpResponse unauthorized(const std::string& m) { return errorJson(401, "Unauthorized", m); }
    static HttpResponse notFound(const std::string& m)     { return errorJson(404, "Not Found", m); }
    static HttpResponse serverError(const std::string& m)  { return errorJson(500, "Internal Server Error", m); }
    static HttpResponse corsOptions() {
        HttpResponse r;
        r.status_code = 204;
        r.reason = "No Content";
        return r;
    }

    std::string toString() const {
        std::string out = "HTTP/1.1 " + std::to_string(status_code) + " " + reason + "\r\n";
        const bool bodyless = (status_code == 101 || status_code == 204);
        for (const auto& [k, v] : headers) out += k + ": " + v + "\r\n";
        if (status_code != 101) {
            out += "Access-Control-Allow-Origin: *\r\n";
            out += "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
            out += "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, PATCH, OPTIONS\r\n";
            if (headers.find("Connection") == headers.end()) out += "Connection: keep-alive\r\n";
        }
        if (!bodyless) out += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        out += "\r\n";
        if (!bodyless) out += body;
        return out;
    }
};

enum class HttpParseResult { COMPLETE, INCOMPLETE, INVALID };

class HttpParser {
public:
    static constexpr size_t MAX_HEADER_BYTES = 16 * 1024;
    static constexpr size_t MAX_BODY_BYTES = 1024 * 1024;

    // Parses one request from the start of `buf`. On COMPLETE, `consumed` is the number of bytes used.
    static HttpParseResult parse(const std::string& buf, HttpRequest& req, size_t& consumed) {
        const size_t headerEnd = buf.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            return buf.size() > MAX_HEADER_BYTES ? HttpParseResult::INVALID : HttpParseResult::INCOMPLETE;
        }
        if (headerEnd > MAX_HEADER_BYTES) return HttpParseResult::INVALID;

        HttpRequest r;
        size_t lineEnd = buf.find("\r\n");
        const std::string requestLine = buf.substr(0, lineEnd);
        const size_t sp1 = requestLine.find(' ');
        const size_t sp2 = requestLine.rfind(' ');
        if (sp1 == std::string::npos || sp2 == sp1) return HttpParseResult::INVALID;
        r.method_text = requestLine.substr(0, sp1);
        r.target = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);
        r.version = requestLine.substr(sp2 + 1);
        if (r.target.empty() || r.version.rfind("HTTP/1.", 0) != 0) return HttpParseResult::INVALID;

        if (r.method_text == "GET") r.method = HttpMethod::GET;
        else if (r.method_text == "POST") r.method = HttpMethod::POST;
        else if (r.method_text == "PUT") r.method = HttpMethod::PUT;
        else if (r.method_text == "DELETE") r.method = HttpMethod::DELETE_;
        else if (r.method_text == "PATCH") r.method = HttpMethod::PATCH;
        else if (r.method_text == "OPTIONS") r.method = HttpMethod::OPTIONS;
        else if (r.method_text == "HEAD") r.method = HttpMethod::HEAD;

        // headers
        size_t pos = lineEnd + 2;
        while (pos < headerEnd + 2) {
            size_t e = buf.find("\r\n", pos);
            if (e == std::string::npos || e > headerEnd + 2) break;
            if (e == pos) break;
            const std::string line = buf.substr(pos, e - pos);
            pos = e + 2;
            const size_t colon = line.find(':');
            if (colon == std::string::npos || colon == 0) return HttpParseResult::INVALID;
            std::string name = toLowerAscii(line.substr(0, colon));
            size_t vs = colon + 1;
            while (vs < line.size() && (line[vs] == ' ' || line[vs] == '\t')) ++vs;
            size_t ve = line.size();
            while (ve > vs && (line[ve - 1] == ' ' || line[ve - 1] == '\t')) --ve;
            r.headers[name] = line.substr(vs, ve - vs);
        }

        if (toLowerAscii(r.getHeader("transfer-encoding")).find("chunked") != std::string::npos)
            return HttpParseResult::INVALID;  // chunked uploads are not supported

        size_t contentLength = 0;
        const std::string cl = r.getHeader("content-length");
        if (!cl.empty()) {
            if (cl.size() > 9 || !std::all_of(cl.begin(), cl.end(), [](unsigned char c) { return std::isdigit(c); }))
                return HttpParseResult::INVALID;
            contentLength = static_cast<size_t>(std::strtoull(cl.c_str(), nullptr, 10));
            if (contentLength > MAX_BODY_BYTES) return HttpParseResult::INVALID;
        }

        const size_t bodyStart = headerEnd + 4;
        if (buf.size() < bodyStart + contentLength) return HttpParseResult::INCOMPLETE;
        r.body = buf.substr(bodyStart, contentLength);

        // path + query
        const size_t q = r.target.find('?');
        r.path = urlDecode(q == std::string::npos ? r.target : r.target.substr(0, q), false);
        if (q != std::string::npos) {
            const std::string qs = r.target.substr(q + 1);
            size_t i = 0;
            while (i <= qs.size()) {
                size_t amp = qs.find('&', i);
                if (amp == std::string::npos) amp = qs.size();
                const std::string pair = qs.substr(i, amp - i);
                if (!pair.empty()) {
                    const size_t eq = pair.find('=');
                    if (eq == std::string::npos) r.query[urlDecode(pair, true)] = "";
                    else r.query[urlDecode(pair.substr(0, eq), true)] = urlDecode(pair.substr(eq + 1), true);
                }
                i = amp + 1;
            }
        }

        consumed = bodyStart + contentLength;
        req = std::move(r);
        return HttpParseResult::COMPLETE;
    }
};

} // namespace http
} // namespace uniconnect
