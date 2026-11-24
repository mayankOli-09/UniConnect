#pragma once
#include "database/UserRepository.h"
#include "http/Router.h"
#include "services/AuthService.h"
#include "services/ChatService.h"

namespace uniconnect {
namespace routes {

// GET /api/groups, GET /api/groups/{id}/messages, POST /api/groups/{id}/messages   (all need a signed-in user)
void registerChatRoutes(http::Router& router, services::ChatService& chat, services::AuthService& auth, database::UserRepository& users);

} // namespace routes
} // namespace uniconnect
