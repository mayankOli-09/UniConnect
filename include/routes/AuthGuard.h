#pragma once
#include "database/UserRepository.h"
#include "http/HttpParser.h"
#include "models/User.h"
#include "services/AuthService.h"

namespace uniconnect {
namespace routes {

// Result of checking the "Authorization: Bearer <token>" header.
struct Caller {
    bool ok = false;
    models::User user;
    http::HttpResponse failure;   // 401 response to send when !ok
};

Caller authenticate(const http::HttpRequest& req, services::AuthService& auth, database::UserRepository& users);

} // namespace routes
} // namespace uniconnect
