#include "routes/AuthGuard.h"

namespace uniconnect {
namespace routes {

Caller authenticate(const http::HttpRequest& req, services::AuthService& auth, database::UserRepository& users) {
    Caller c;
    c.failure = http::HttpResponse::unauthorized("Sign in to continue.");
    const std::string header = req.getHeader("authorization");
    const std::string prefix = "Bearer ";
    if (header.size() <= prefix.size() || http::toLowerAscii(header.substr(0, prefix.size())) != http::toLowerAscii(prefix)) return c;

    const auto claims = auth.verifyToken(header.substr(prefix.size()));
    if (!claims) {
        c.failure = http::HttpResponse::unauthorized("Your session has expired. Please sign in again.");
        return c;
    }
    const auto user = users.findById(claims->user_id);
    if (!user) return c;                // the account no longer exists
    c.ok = true;
    c.user = *user;
    c.user.password_hash.clear();
    return c;
}

} // namespace routes
} // namespace uniconnect
