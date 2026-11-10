#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <functional>
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "websocket/WebSocketParser.h"
#include "utils/Json.h"

namespace uniconnect {
namespace websocket {

class WebSocketSession {
public:
    explicit WebSocketSession(int fd, std::string session_id);

    int getFd() const { return fd_; }
    const std::string& getSessionId() const { return session_id_; }

    void setUserId(const std::string& uid) { user_id_ = uid; }
    const std::string& getUserId() const { return user_id_; }

    void subscribe(const std::string& group_id);
    void unsubscribe(const std::string& group_id);
    bool isSubscribed(const std::string& group_id) const;
    const std::set<std::string>& getSubscribedGroups() const { return subscribed_groups_; }

    // Buffer for streaming frame reassembly
    std::vector<uint8_t>& getRxBuffer() { return rx_buffer_; }

private:
    int fd_;
    std::string session_id_;
    std::string user_id_;
    std::set<std::string> subscribed_groups_;
    std::vector<uint8_t> rx_buffer_;
};

using SendCallback = std::function<void(int fd, const std::string& data)>;

class WebSocketManager {
public:
    static WebSocketManager& getInstance();

    // Validate incoming handshake HTTP request and construct 101 response
    bool handleHandshake(const http::HttpRequest& request, http::HttpResponse& response);

    // Register a new WebSocket connection
    std::shared_ptr<WebSocketSession> addSession(int client_fd);

    // Remove connection
    void removeSession(int client_fd);

    // Retrieve session
    std::shared_ptr<WebSocketSession> getSession(int client_fd);

    // Group subscriptions
    void subscribe(int client_fd, const std::string& group_id);
    void unsubscribe(int client_fd, const std::string& group_id);

    // Send callback configuration (e.g. writes to socket via EpollServer)
    void setSendCallback(SendCallback cb);

    // Broadcast primitives
    void sendText(int client_fd, const std::string& message);
    void broadcastToGroup(const std::string& group_id, const std::string& message);
    void broadcastToUser(const std::string& user_id, const std::string& message);
    void broadcastToAll(const std::string& message);

    size_t activeSessionsCount() const;
    uint64_t totalBroadcastsCount() const;

private:
    WebSocketManager() = default;
    mutable std::mutex mutex_;
    std::map<int, std::shared_ptr<WebSocketSession>> sessions_;
    std::map<std::string, std::set<int>> group_subscribers_;
    SendCallback send_cb_;
    uint64_t total_broadcasts_{0};
};

} // namespace websocket
} // namespace uniconnect
