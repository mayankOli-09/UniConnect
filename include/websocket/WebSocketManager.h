#pragma once
// Tracks the open WebSocket connections: who is signed in on each and which groups they have joined.
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "models/User.h"
#include "websocket/WebSocket.h"

namespace uniconnect {
namespace websocket {

class WebSocketManager {
public:
    using Sender = std::function<void(int fd, const std::string& bytes)>;
    static constexpr size_t MAX_SUBSCRIPTIONS = 100;

    explicit WebSocketManager(Sender sender) : send_(std::move(sender)) {}

    void addSession(int fd) { std::lock_guard<std::mutex> l(m_); sessions_[fd]; }
    // Returns the user's id when this was their last open connection (they just went offline).
    std::optional<int> removeSession(int fd) {
        std::lock_guard<std::mutex> l(m_);
        auto it = sessions_.find(fd);
        if (it == sessions_.end()) return std::nullopt;
        const bool signedIn = it->second.signed_in;
        const int uid = it->second.user.id;
        sessions_.erase(it);
        if (signedIn && !onlineLocked(uid)) return uid;
        return std::nullopt;
    }
    bool isOnline(int user) const { std::lock_guard<std::mutex> l(m_); return onlineLocked(user); }
    std::vector<int> onlineUserIds() const {
        std::lock_guard<std::mutex> l(m_);
        std::set<int> ids;
        for (const auto& [fd, s] : sessions_) if (s.signed_in) ids.insert(s.user.id);
        return {ids.begin(), ids.end()};
    }
    // Sends to every signed-in connection (used for online/offline announcements).
    size_t broadcastToAll(const std::string& text) {
        std::vector<int> targets;
        {
            std::lock_guard<std::mutex> l(m_);
            for (const auto& [fd, s] : sessions_) if (s.signed_in) targets.push_back(fd);
        }
        const std::string frame = WebSocketParser::createTextFrame(text);
        for (int fd : targets) send_(fd, frame);
        return targets.size();
    }
    size_t sessionCount() const { std::lock_guard<std::mutex> l(m_); return sessions_.size(); }

    // Returns true when this made the user newly online (their first signed-in connection).
    bool authenticate(int fd, const models::User& user) {
        std::lock_guard<std::mutex> l(m_);
        auto it = sessions_.find(fd);
        if (it == sessions_.end()) return false;
        const bool wasOnline = onlineLocked(user.id);
        it->second.user = user;
        it->second.signed_in = true;
        return !wasOnline;
    }
    bool userOf(int fd, models::User& out) const {
        std::lock_guard<std::mutex> l(m_);
        auto it = sessions_.find(fd);
        if (it == sessions_.end() || !it->second.signed_in) return false;
        out = it->second.user;
        return true;
    }
    // false when the session is unknown or has joined too many groups
    bool subscribe(int fd, int group) {
        std::lock_guard<std::mutex> l(m_);
        auto it = sessions_.find(fd);
        if (it == sessions_.end()) return false;
        if (it->second.groups.size() >= MAX_SUBSCRIPTIONS && !it->second.groups.count(group)) return false;
        it->second.groups.insert(group);
        return true;
    }

    void sendText(int fd, const std::string& text) { send_(fd, WebSocketParser::createTextFrame(text)); }
    void sendRaw(int fd, const std::string& bytes) { send_(fd, bytes); }

    // Sends to every connection that joined `group` AND whose user is still in `recipients`
    // (so someone who left the group stops receiving even if their socket is still open).
    size_t broadcastToGroup(int group, const std::string& text, const std::vector<int>& recipients) {
        std::vector<int> targets;
        {
            std::lock_guard<std::mutex> l(m_);
            const std::set<int> allowed(recipients.begin(), recipients.end());
            for (const auto& [fd, s] : sessions_)
                if (s.signed_in && s.groups.count(group) && allowed.count(s.user.id)) targets.push_back(fd);
        }
        const std::string frame = WebSocketParser::createTextFrame(text);
        for (int fd : targets) send_(fd, frame);
        return targets.size();
    }

    // Sends to every connection signed in as one of these users (for notifications).
    size_t broadcastToUsers(const std::vector<int>& users, const std::string& text) {
        std::vector<int> targets;
        {
            std::lock_guard<std::mutex> l(m_);
            const std::set<int> wanted(users.begin(), users.end());
            for (const auto& [fd, s] : sessions_) if (s.signed_in && wanted.count(s.user.id)) targets.push_back(fd);
        }
        const std::string frame = WebSocketParser::createTextFrame(text);
        for (int fd : targets) send_(fd, frame);
        return targets.size();
    }

private:
    bool onlineLocked(int user) const {
        for (const auto& [fd, s] : sessions_) if (s.signed_in && s.user.id == user) return true;
        return false;
    }
    struct Session { models::User user; bool signed_in = false; std::set<int> groups; };
    Sender send_;
    mutable std::mutex m_;
    std::map<int, Session> sessions_;
};

} // namespace websocket
} // namespace uniconnect
