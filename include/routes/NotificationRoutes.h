#pragma once
#include "database/UserRepository.h"
#include "http/Router.h"
#include "services/AuthService.h"
#include "services/NotificationService.h"

namespace uniconnect {
namespace routes {

// GET /api/notifications, POST /api/announcements, POST /api/notifications/read   (all need a signed-in user)
void registerNotificationRoutes(http::Router& router, services::NotificationService& svc, services::AuthService& auth, database::UserRepository& users);

} // namespace routes
} // namespace uniconnect
