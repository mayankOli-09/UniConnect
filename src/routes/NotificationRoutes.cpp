#include "routes/NotificationRoutes.h"

#include "routes/AuthGuard.h"

namespace uniconnect {
namespace routes {

using http::HttpRequest;
using http::HttpResponse;
using http::RouteParams;

namespace {
const char* reasonFor(int status) {
    switch (status) {
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 429: return "Too Many Requests";
        case 503: return "Service Unavailable";
        default:  return "Internal Server Error";
    }
}
HttpResponse toResponse(const services::ServiceResult& r, bool created = false) {
    if (r.status >= 400) return HttpResponse::errorJson(r.status, reasonFor(r.status), r.error);
    return created ? HttpResponse::created(r.body) : HttpResponse::okJson(r.body);
}
} // namespace

void registerNotificationRoutes(http::Router& router, services::NotificationService& svc, services::AuthService& auth, database::UserRepository& users) {
    router.get("/api/notifications", [&](const HttpRequest& req, const RouteParams&) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        return toResponse(svc.list(who.user, req.getQueryParam("user_id")));
    });
    router.post("/api/announcements", [&](const HttpRequest& req, const RouteParams&) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        const auto body = req.getJsonBody();
        if (body.size() == 0) return HttpResponse::badRequest("Request body must be a JSON object.");
        return toResponse(svc.announce(who.user, body), true);
    });
    router.post("/api/notices", [&](const HttpRequest& req, const RouteParams&) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        const auto body = req.getJsonBody();
        if (body.size() == 0) return HttpResponse::badRequest("Request body must be a JSON object.");
        return toResponse(svc.notice(who.user, body), true);
    });
    router.post("/api/notifications/read", [&](const HttpRequest& req, const RouteParams&) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        const auto body = req.getJsonBody();
        if (body.size() == 0) return HttpResponse::badRequest("Request body must be a JSON object.");
        return toResponse(svc.markRead(who.user, body));
    });
}

} // namespace routes
} // namespace uniconnect
