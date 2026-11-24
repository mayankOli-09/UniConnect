#pragma once
// Maps (method, path pattern) to handlers. Patterns may contain {name} segments, e.g. /api/groups/{id}/messages.
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "http/HttpParser.h"

namespace uniconnect {
namespace http {

using RouteParams = std::map<std::string, std::string>;
using RouteHandler = std::function<HttpResponse(const HttpRequest&, const RouteParams&)>;

class Router {
public:
    void add(HttpMethod method, const std::string& pattern, RouteHandler handler) {
        routes_.push_back({method, split(pattern), std::move(handler)});
    }
    void get(const std::string& p, RouteHandler h)  { add(HttpMethod::GET, p, std::move(h)); }
    void post(const std::string& p, RouteHandler h) { add(HttpMethod::POST, p, std::move(h)); }

    struct Match {
        enum class Kind { FOUND, METHOD_NOT_ALLOWED, NOT_FOUND } kind = Kind::NOT_FOUND;
        RouteHandler handler;
        RouteParams params;
        std::string allow;   // for METHOD_NOT_ALLOWED
    };

    Match match(HttpMethod method, const std::string& path) const {
        Match m;
        const auto segs = split(path);
        for (const auto& r : routes_) {
            RouteParams params;
            if (!matches(r.segments, segs, params)) continue;
            if (r.method == method) {
                m.kind = Match::Kind::FOUND;
                m.handler = r.handler;
                m.params = std::move(params);
                return m;
            }
            m.kind = Match::Kind::METHOD_NOT_ALLOWED;
            if (!m.allow.empty()) m.allow += ", ";
            m.allow += methodName(r.method);
        }
        return m;
    }

private:
    struct Route {
        HttpMethod method;
        std::vector<std::string> segments;
        RouteHandler handler;
    };

    static std::vector<std::string> split(const std::string& path) {
        std::vector<std::string> out;
        size_t i = 0;
        while (i <= path.size()) {
            size_t j = path.find('/', i);
            if (j == std::string::npos) j = path.size();
            if (j > i) out.push_back(path.substr(i, j - i));
            i = j + 1;
        }
        return out;
    }
    static bool matches(const std::vector<std::string>& pattern, const std::vector<std::string>& segs, RouteParams& params) {
        if (pattern.size() != segs.size()) return false;
        for (size_t i = 0; i < pattern.size(); ++i) {
            const auto& p = pattern[i];
            if (p.size() > 2 && p.front() == '{' && p.back() == '}') params[p.substr(1, p.size() - 2)] = segs[i];
            else if (p != segs[i]) return false;
        }
        return true;
    }
    static const char* methodName(HttpMethod m) {
        switch (m) {
            case HttpMethod::GET: return "GET";
            case HttpMethod::POST: return "POST";
            case HttpMethod::PUT: return "PUT";
            case HttpMethod::DELETE_: return "DELETE";
            case HttpMethod::PATCH: return "PATCH";
            default: return "GET";
        }
    }
    std::vector<Route> routes_;
};

} // namespace http
} // namespace uniconnect
