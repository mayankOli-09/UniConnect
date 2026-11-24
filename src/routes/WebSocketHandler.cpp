#include "routes/WebSocketHandler.h"

#include "utils/Logger.h"

namespace uniconnect {
namespace routes {

using utils::JsonValue;

WebSocketHandler::WebSocketHandler(services::AuthService& auth, database::UserRepository& users, services::ChatService* chat,
                                   websocket::WebSocketManager& manager, std::function<void(int)> close_client)
    : auth_(auth), users_(users), chat_(chat), manager_(manager), close_client_(std::move(close_client)) {}

http::HttpResponse WebSocketHandler::handshake(int fd, const http::HttpRequest& req, bool& accepted) {
    accepted = false;
    if (req.method != http::HttpMethod::GET) return http::HttpResponse::badRequest("WebSocket upgrade must use GET.");
    if (req.getHeader("sec-websocket-version") != "13") {
        auto r = http::HttpResponse::errorJson(426, "Upgrade Required", "Unsupported WebSocket version.");
        r.setHeader("Sec-WebSocket-Version", "13");
        return r;
    }
    const std::string key = req.getHeader("sec-websocket-key");
    if (key.size() != 24) return http::HttpResponse::badRequest("Missing or invalid Sec-WebSocket-Key.");

    http::HttpResponse r;
    r.status_code = 101;
    r.reason = "Switching Protocols";
    r.setHeader("Upgrade", "websocket");
    r.setHeader("Connection", "Upgrade");
    r.setHeader("Sec-WebSocket-Accept", websocket::computeAcceptKey(key));
    manager_.addSession(fd);
    accepted = true;
    return r;
}

void WebSocketHandler::sendEvent(int fd, const JsonValue& event) { manager_.sendText(fd, event.serialize(false)); }

void WebSocketHandler::sendError(int fd, int code, const std::string& message) {
    auto e = JsonValue::object();
    e["event"] = "error";
    e["code"] = code;
    e["message"] = message;
    sendEvent(fd, e);
}

void WebSocketHandler::onDisconnect(int fd) {
    if (const auto gone = manager_.removeSession(fd)) {          // their last connection closed: tell everyone they are offline
        auto e = JsonValue::object();
        auto d = JsonValue::object();
        d["user_id"] = *gone;
        d["online"] = false;
        e["event"] = "presence";
        e["data"] = d;
        manager_.broadcastToAll(e.serialize(false));
    }
}

void WebSocketHandler::onFrame(int fd, const websocket::WsFrame& frame) {
    using websocket::WsOpCode;
    if (frame.opcode == WsOpCode::PING) { manager_.sendRaw(fd, websocket::WebSocketParser::createPongFrame(frame.payload)); return; }
    if (frame.opcode == WsOpCode::CLOSE) { manager_.sendRaw(fd, websocket::WebSocketParser::createCloseFrame()); close_client_(fd); return; }
    if (frame.opcode != WsOpCode::TEXT) return;   // binary and fragmented messages are not used by this protocol

    JsonValue msg;
    if (!JsonValue::tryParse(frame.payload, msg) || !msg.isObject()) { sendError(fd, 400, "Messages must be JSON objects."); return; }
    const std::string action = msg["action"].asString();

    if (action == "authenticate") {
        const auto claims = auth_.verifyToken(msg["token"].asString());
        const auto user = claims ? users_.findById(claims->user_id) : std::nullopt;
        if (!user) { sendError(fd, 401, "Invalid or expired token."); return; }
        models::User u = *user;
        u.password_hash.clear();
        const bool cameOnline = manager_.authenticate(fd, u);
        auto ack = JsonValue::object();
        ack["event"] = "authenticated";
        ack["user_id"] = u.id;
        sendEvent(fd, ack);
        auto snap = JsonValue::object();                           // who is online right now
        auto ids = JsonValue::array();
        for (int id : manager_.onlineUserIds()) ids.push_back(id);
        auto sd = JsonValue::object();
        sd["user_ids"] = ids;
        snap["event"] = "presence_snapshot";
        snap["data"] = sd;
        sendEvent(fd, snap);
        if (cameOnline) {
            auto e = JsonValue::object();
            auto d = JsonValue::object();
            d["user_id"] = u.id;
            d["online"] = true;
            e["event"] = "presence";
            e["data"] = d;
            manager_.broadcastToAll(e.serialize(false));
        }
        return;
    }

    models::User user;
    if (!manager_.userOf(fd, user)) { sendError(fd, 401, "Authenticate first."); return; }

    int groupId = 0;
    const std::string groupText = msg["group_id"].asString();
    const bool validGroup = !groupText.empty() && groupText.size() <= 9 && groupText.find_first_not_of("0123456789") == std::string::npos && (groupId = std::atoi(groupText.c_str())) > 0;

    if (action == "subscribe") {
        if (!chat_) { sendError(fd, 503, "Chat is not available."); return; }
        if (!validGroup) { sendError(fd, 400, "Invalid group id."); return; }
        if (!chat_->isMember(user.id, groupId)) { sendError(fd, 404, "Group not found."); return; }
        if (!manager_.subscribe(fd, groupId)) { sendError(fd, 400, "Too many subscriptions."); return; }
        auto ack = JsonValue::object();
        ack["event"] = "subscribed";
        ack["group_id"] = groupId;
        sendEvent(fd, ack);
    } else if (action == "typing") {
        if (!chat_) return;
        if (!validGroup || !chat_->isMember(user.id, groupId)) { sendError(fd, 404, "Group not found."); return; }
        std::vector<int> others;
        for (int id : chat_->members(groupId)) if (id != user.id) others.push_back(id);
        auto e = JsonValue::object();
        auto d = JsonValue::object();
        d["group_id"] = groupId;
        d["user_id"] = user.id;
        d["name"] = user.name;
        d["typing"] = msg["typing"].asBool(true);
        e["event"] = "typing";
        e["data"] = d;
        manager_.broadcastToGroup(groupId, e.serialize(false), others);   // only people who have this chat open, never the sender
    } else if (action == "send_message") {
        if (!chat_) { sendError(fd, 503, "Chat is not available."); return; }
        if (!validGroup) { sendError(fd, 400, "Invalid group id."); return; }
        const auto r = chat_->send(user, groupText, msg, false);   // queued; everyone, including the sender, gets "new_message"
        if (r.status >= 400) sendError(fd, r.status, r.error);
    } else {
        sendError(fd, 400, "Unknown action.");
    }
}

} // namespace routes
} // namespace uniconnect
