#include "routes/OsRoutes.h"

#include <functional>

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
        case 429: return "Too Many Requests";
        default:  return "Internal Server Error";
    }
}
} // namespace

void registerOsRoutes(http::Router& router, services::OsService& os, services::AuthService& auth, database::UserRepository& users) {
    router.get("/api/os/stats", [&](const HttpRequest& req, const RouteParams&) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        return HttpResponse::okJson(os.stats().body);
    });

    using Action = std::function<services::ServiceResult(const models::User&, const utils::JsonValue&)>;
    auto admin = [&](Action action) {
        return [&, action](const HttpRequest& req, const RouteParams&) {
            const auto who = authenticate(req, auth, users);
            if (!who.ok) return who.failure;
            const auto body = req.getJsonBody();
            if (!req.body.empty() && body.size() == 0) return HttpResponse::badRequest("Request body must be a JSON object.");
            const auto r = action(who.user, body);
            if (r.status >= 400) return HttpResponse::errorJson(r.status, reasonFor(r.status), r.error);
            return HttpResponse::okJson(r.body);
        };
    };
    router.post("/api/os/scheduler", admin([&](const models::User& u, const utils::JsonValue& b) { return os.setScheduler(u, b); }));
    router.post("/api/os/deadlock-test", admin([&](const models::User& u, const utils::JsonValue& b) { return os.deadlockTest(u, b); }));
    router.post("/api/os/load-test", admin([&](const models::User& u, const utils::JsonValue& b) { return os.loadTest(u, b); }));
}

} // namespace routes
} // namespace uniconnect
