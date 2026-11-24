#pragma once
#include "http/Router.h"
#include "services/AuthService.h"

namespace uniconnect {
namespace routes {

// POST /api/auth/login, POST /api/auth/register
void registerAuthRoutes(http::Router& router, services::AuthService& auth);

} // namespace routes
} // namespace uniconnect
