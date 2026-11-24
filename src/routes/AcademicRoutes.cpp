#include "routes/AcademicRoutes.h"

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
        case 409: return "Conflict";
        case 503: return "Service Unavailable";
        default:  return "Internal Server Error";
    }
}
} // namespace

void registerAcademicRoutes(http::Router& router, services::AcademicService& academics,
                            services::AuthService& auth, database::UserRepository& users) {
    router.get("/api/courses", [&](const HttpRequest& req, const RouteParams&) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        return HttpResponse::okJson(academics.courses());
    });

    router.get("/api/hierarchy", [&](const HttpRequest& req, const RouteParams&) {
        const auto who = authenticate(req, auth, users);
        if (!who.ok) return who.failure;
        return HttpResponse::okJson(academics.hierarchy());
    });

    auto change = [&](bool enroll) {
        return [&, enroll](const HttpRequest& req, const RouteParams&) {
            const auto who = authenticate(req, auth, users);
            if (!who.ok) return who.failure;
            const auto body = req.getJsonBody();
            if (!req.body.empty() && body.size() == 0) return HttpResponse::badRequest("Request body must be a JSON object.");
            const std::string student = body["student_id"].asString(), section = body["section_id"].asString(), course = body["course_id"].asString();
            const auto r = enroll ? academics.enroll(who.user, student, section, course) : academics.drop(who.user, student, section, course);
            if (r.status >= 400) return HttpResponse::errorJson(r.status, reasonFor(r.status), r.error);
            return HttpResponse::okJson(r.body);
        };
    };
    router.post("/api/enroll", change(true));
    router.post("/api/drop", change(false));
}

} // namespace routes
} // namespace uniconnect
