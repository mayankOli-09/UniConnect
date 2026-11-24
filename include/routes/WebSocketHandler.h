#pragma once
#include <functional>

#include "database/UserRepository.h"
#include "http/HttpParser.h"
#include "services/AuthService.h"
#include "services/ChatService.h"
#include "websocket/WebSocketManager.h"

namespace uniconnect {
namespace routes {

// The /ws protocol. Client -> server (JSON text frames):
//   {"action":"authenticate","token":"..."}                    -> {"event":"authenticated","user_id":N}
//   {"action":"subscribe","group_id":N}                        -> {"event":"subscribed","group_id":N}
//   {"action":"send_message","group_id":N,"content":"...","priority":1..10}
// Server -> client: {"event":"new_message","data":{...}}, {"event":"notification","data":{...}},
//                   {"event":"error","code":N,"message":"..."}
class WebSocketHandler {
public:
    WebSocketHandler(services::AuthService& auth, database::UserRepository& users, services::ChatService* chat,
                     websocket::WebSocketManager& manager, std::function<void(int)> close_client);

    // Validates the upgrade request and builds the 101 response. `accepted` tells the caller whether to keep the connection.
    http::HttpResponse handshake(int fd, const http::HttpRequest& req, bool& accepted);
    void onFrame(int fd, const websocket::WsFrame& frame);
    void onDisconnect(int fd);

private:
    void sendError(int fd, int code, const std::string& message);
    void sendEvent(int fd, const utils::JsonValue& event);

    services::AuthService& auth_;
    database::UserRepository& users_;
    services::ChatService* chat_;
    websocket::WebSocketManager& manager_;
    std::function<void(int)> close_client_;
};

} // namespace routes
} // namespace uniconnect
