#include "routes/ChatRoutes.h"

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
        default:  return "Internal Server Error";
    }
}
HttpResponse toResponse(const services::ServiceResult& r, bool created = false) {
    if (r.status >= 400) return HttpResponse::errorJson(r.status, reasonFor(r.status), r.error);
    return created ? HttpResponse::created(r.body) : HttpResponse::okJson(r.body);
}
} // namespace

void registerChatRoutes(http::Router& router, services::ChatService& chat, services::AuthService& auth, database::UserRepository& users) {
    router.get("/api/groups", [&](const HttpRequest& req, const RouteParams&) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        return toResponse(chat.groups(who.user, req.getQueryParam("user_id")));
    });

    router.get("/api/groups/{id}/messages", [&](const HttpRequest& req, const RouteParams& p) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        return toResponse(chat.messages(who.user, p.at("id"), req.getQueryParam("limit"), req.getQueryParam("before_id")));
    });

    router.post("/api/groups/{id}/messages", [&](const HttpRequest& req, const RouteParams& p) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        const auto body = req.getJsonBody();
        if (body.size() == 0) return HttpResponse::badRequest("Request body must be a JSON object.");
        return toResponse(chat.send(who.user, p.at("id"), body), true);
    });

    router.get("/api/users", [&](const HttpRequest& req, const RouteParams&) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        return toResponse(chat.directory(who.user, req.getQueryParam("q")));
    });
    router.post("/api/chats/direct", [&](const HttpRequest& req, const RouteParams&) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        const auto body = req.getJsonBody();
        if (body.size() == 0) return HttpResponse::badRequest("Request body must be a JSON object.");
        return toResponse(chat.startDirect(who.user, body));
    });
    router.post("/api/groups", [&](const HttpRequest& req, const RouteParams&) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        const auto body = req.getJsonBody();
        if (body.size() == 0) return HttpResponse::badRequest("Request body must be a JSON object.");
        return toResponse(chat.createGroup(who.user, body), true);
    });
    router.post("/api/groups/{id}/leave", [&](const HttpRequest& req, const RouteParams& p) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        return toResponse(chat.leave(who.user, p.at("id")));
    });
    router.post("/api/groups/{id}/mute", [&](const HttpRequest& req, const RouteParams& p) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        const auto body = req.getJsonBody();
        if (body.size() == 0) return HttpResponse::badRequest("Request body must be a JSON object.");
        return toResponse(chat.mute(who.user, p.at("id"), body));
    });
    router.post("/api/groups/{id}/read", [&](const HttpRequest& req, const RouteParams& p) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        return toResponse(chat.markRead(who.user, p.at("id"), req.getJsonBody()));   // an empty body means "up to the newest message"
    });
}

} // namespace routes
} // namespace uniconnect
