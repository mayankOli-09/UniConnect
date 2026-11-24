#include "routes/AuthRoutes.h"

#include "models/User.h"

namespace uniconnect {
namespace routes {

using http::HttpMethod;
using http::HttpRequest;
using http::HttpResponse;
using http::RouteParams;

namespace {

const char* reasonFor(int status) {
    switch (status) {
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 409: return "Conflict";
        case 429: return "Too Many Requests";
        case 503: return "Service Unavailable";
        default:  return "Internal Server Error";
    }
}

HttpResponse errorFrom(const services::AuthResult& r) {
    return HttpResponse::errorJson(r.status, reasonFor(r.status), r.error_message);
}

// The body must be a JSON object; anything else is a client error.
bool readBody(const HttpRequest& req, utils::JsonValue& out) {
    if (req.body.empty() || !utils::JsonValue::tryParse(req.body, out) || !out.isObject()) return false;
    return true;
}

} // namespace

void registerAuthRoutes(http::Router& router, services::AuthService& auth) {
    router.post("/api/auth/login", [&auth](const HttpRequest& req, const RouteParams&) {
        utils::JsonValue body;
        if (!readBody(req, body)) return HttpResponse::badRequest("Request body must be a JSON object.");
        const auto r = auth.login(body["email"].asString(), body["password"].asString());
        if (!r.success) return errorFrom(r);
        return HttpResponse::okJson(r.toJson());
    });

    router.post("/api/auth/register", [&auth](const HttpRequest& req, const RouteParams&) {
        utils::JsonValue body;
        if (!readBody(req, body)) return HttpResponse::badRequest("Request body must be a JSON object.");
        const auto r = auth.registerUser(
            body["name"].asString(), body["email"].asString(), body["password"].asString(),
            models::stringToUserRole(body["role"].asString("STUDENT")),
            body["department_id"].asString(), body["section_id"].asString());
        if (!r.success) return errorFrom(r);
        return HttpResponse::created(r.toJson());
    });
}

} // namespace routes
} // namespace uniconnect
