#pragma once
#include "database/UserRepository.h"
#include "http/Router.h"
#include "services/AuthService.h"
#include "services/OsService.h"

namespace uniconnect {
namespace routes {

// GET /api/os/stats (signed in), POST /api/os/scheduler, /api/os/deadlock-test, /api/os/load-test (administrators)
void registerOsRoutes(http::Router& router, services::OsService& os, services::AuthService& auth, database::UserRepository& users);

} // namespace routes
} // namespace uniconnect
