#include "websocket/WebSocket.h"
#include "utils/Crypto.h"
#include "utils/UUID.h"
#include "utils/Logger.h"

namespace uniconnect {
namespace websocket {

WebSocketSession::WebSocketSession(int fd, std::string session_id)
    : fd_(fd), session_id_(std::move(session_id)) {}

void WebSocketSession::subscribe(const std::string& group_id) {
    subscribed_groups_.insert(group_id);
}

void WebSocketSession::unsubscribe(const std::string& group_id) {
    subscribed_groups_.erase(group_id);
}

bool WebSocketSession::isSubscribed(const std::string& group_id) const {
    return subscribed_groups_.find(group_id) != subscribed_groups_.end();
}

WebSocketManager& WebSocketManager::getInstance() {
    static WebSocketManager instance;
    return instance;
}

bool WebSocketManager::handleHandshake(const http::HttpRequest& request, http::HttpResponse& response) {
    if (!request.isWebSocketUpgrade()) {
        return false;
    }

    std::string client_key = request.getWebSocketKey();
    if (client_key.empty()) {
        response = http::HttpResponse::badRequest("Missing Sec-WebSocket-Key header");
        return false;
    }

    std::string accept_key = utils::Crypto::generateWebSocketAcceptKey(client_key);
    response = http::HttpResponse::webSocketHandshake(accept_key);
    return true;
}

std::shared_ptr<WebSocketSession> WebSocketManager::addSession(int client_fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string sid = utils::UUID::generate();
    auto session = std::make_shared<WebSocketSession>(client_fd, sid);
    sessions_[client_fd] = session;
    utils::Logger::getInstance().info("WebSocket", "Session registered for fd:", client_fd, "sid:", sid);
    return session;
}

void WebSocketManager::removeSession(int client_fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(client_fd);
    if (it != sessions_.end()) {
        for (const auto& group_id : it->second->getSubscribedGroups()) {
            auto git = group_subscribers_.find(group_id);
            if (git != group_subscribers_.end()) {
                git->second.erase(client_fd);
                if (git->second.empty()) {
                    group_subscribers_.erase(git);
                }
            }
        }
        sessions_.erase(it);
        utils::Logger::getInstance().info("WebSocket", "Session removed for fd:", client_fd);
    }
}

std::shared_ptr<WebSocketSession> WebSocketManager::getSession(int client_fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(client_fd);
    return it != sessions_.end() ? it->second : nullptr;
}

void WebSocketManager::subscribe(int client_fd, const std::string& group_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(client_fd);
    if (it != sessions_.end()) {
        it->second->subscribe(group_id);
        group_subscribers_[group_id].insert(client_fd);
        utils::Logger::getInstance().debug("WebSocket", "fd:", client_fd, "subscribed to group:", group_id);
    }
}

void WebSocketManager::unsubscribe(int client_fd, const std::string& group_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(client_fd);
    if (it != sessions_.end()) {
        it->second->unsubscribe(group_id);
        auto git = group_subscribers_.find(group_id);
        if (git != group_subscribers_.end()) {
            git->second.erase(client_fd);
        }
    }
}

void WebSocketManager::setSendCallback(SendCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    send_cb_ = std::move(cb);
}

void WebSocketManager::sendText(int client_fd, const std::string& message) {
    std::string frame = WebSocketParser::encodeFrame(message, WsOpCode::TEXT, true);
    SendCallback cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = send_cb_;
    }
    if (cb) {
        cb(client_fd, frame);
    }
}

void WebSocketManager::broadcastToGroup(const std::string& group_id, const std::string& message) {
    std::string frame = WebSocketParser::encodeFrame(message, WsOpCode::TEXT, true);
    std::vector<int> target_fds;
    SendCallback cb;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        total_broadcasts_++;
        cb = send_cb_;
        auto git = group_subscribers_.find(group_id);
        if (git != group_subscribers_.end()) {
            target_fds.assign(git->second.begin(), git->second.end());
        }
    }

    if (cb) {
        for (int fd : target_fds) {
            cb(fd, frame);
        }
    }
}

void WebSocketManager::broadcastToUser(const std::string& user_id, const std::string& message) {
    std::string frame = WebSocketParser::encodeFrame(message, WsOpCode::TEXT, true);
    std::vector<int> target_fds;
    SendCallback cb;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        total_broadcasts_++;
        cb = send_cb_;
        for (const auto& [fd, session] : sessions_) {
            if (session->getUserId() == user_id) {
                target_fds.push_back(fd);
            }
        }
    }

    if (cb) {
        for (int fd : target_fds) {
            cb(fd, frame);
        }
    }
}

void WebSocketManager::broadcastToAll(const std::string& message) {
    std::string frame = WebSocketParser::encodeFrame(message, WsOpCode::TEXT, true);
    std::vector<int> target_fds;
    SendCallback cb;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        total_broadcasts_++;
        cb = send_cb_;
        for (const auto& [fd, _] : sessions_) {
            target_fds.push_back(fd);
        }
    }

    if (cb) {
        for (int fd : target_fds) {
            cb(fd, frame);
        }
    }
}

size_t WebSocketManager::activeSessionsCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

uint64_t WebSocketManager::totalBroadcastsCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_broadcasts_;
}

} // namespace websocket
} // namespace uniconnect
