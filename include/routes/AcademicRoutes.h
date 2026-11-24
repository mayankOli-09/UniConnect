#pragma once
#include "database/UserRepository.h"
#include "http/Router.h"
#include "services/AcademicService.h"
#include "services/AuthService.h"

namespace uniconnect {
namespace routes {

// GET /api/courses, GET /api/hierarchy, POST /api/enroll, POST /api/drop   (all need a signed-in user)
void registerAcademicRoutes(http::Router& router, services::AcademicService& academics,
                            services::AuthService& auth, database::UserRepository& users);

} // namespace routes
} // namespace uniconnect
