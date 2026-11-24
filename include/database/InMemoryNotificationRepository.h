#pragma once
// Non-persistent NotificationRepository for the HTTP tests; group membership comes from an InMemoryChatRepository.
#include <algorithm>
#include <map>
#include <set>
#include <mutex>

#include "database/InMemoryChatRepository.h"
#include "database/NotificationRepository.h"

namespace uniconnect {
namespace database {

class InMemoryNotificationRepository : public NotificationRepository {
public:
    explicit InMemoryNotificationRepository(InMemoryChatRepository& chat) : chat_(chat) {}

    std::vector<models::Notification> forUser(int uid, int limit) override {
        std::lock_guard<std::mutex> l(m_);
        std::vector<models::Notification> out;
        for (auto it = items_.rbegin(); it != items_.rend() && static_cast<int>(out.size()) < limit; ++it) if (it->user_id == uid) out.push_back(*it);
        return out;
    }
    int unreadCount(int uid) override {
        std::lock_guard<std::mutex> l(m_);
        return static_cast<int>(std::count_if(items_.begin(), items_.end(), [&](const auto& n) { return n.user_id == uid && !n.is_read; }));
    }
    void setRole(int user, const std::string& role) { std::lock_guard<std::mutex> l(m_); roles_[user] = role; }
    std::optional<AnnouncementResult> createNotice(int sender, const std::string& audience, const std::string& title, const std::string& content) override {
        std::lock_guard<std::mutex> l(m_);
        AnnouncementResult r;
        r.announcement_id = next_announcement_++;
        for (const auto& [uid, role] : roles_) {
            const bool wanted = (audience == "STUDENTS" && role == "STUDENT") || (audience == "FACULTY" && role == "FACULTY") ||
                                (audience == "BOTH" && (role == "STUDENT" || role == "FACULTY"));
            if (!wanted || uid == sender) continue;
            models::Notification n;
            n.id = next_++; n.user_id = uid; n.announcement_id = r.announcement_id; n.title = title; n.content = content; n.kind = "NOTICE"; n.created_at = "2026-10-28T10:00:00Z";
            items_.push_back(n);
            r.notifications.push_back(n);
        }
        return r;
    }
    std::vector<models::Notification> notifyUsers(const std::vector<int>& ids, const std::string& kind, const std::string& title, const std::string& content) override {
        std::lock_guard<std::mutex> l(m_);
        std::vector<models::Notification> out;
        for (int uid : std::set<int>(ids.begin(), ids.end())) {
            models::Notification n;
            n.id = next_++; n.user_id = uid; n.title = title; n.content = content; n.kind = kind; n.created_at = "2026-10-28T10:00:00Z";
            items_.push_back(n);
            out.push_back(n);
        }
        return out;
    }
    bool groupExists(int gid) override { return !chat_.memberIds(gid).empty(); }
    std::optional<AnnouncementResult> createAnnouncement(int sender, int gid, const std::string& title, const std::string& content) override {
        std::lock_guard<std::mutex> l(m_);
        AnnouncementResult r;
        r.announcement_id = next_announcement_++;
        for (int uid : chat_.memberIds(gid)) {
            if (uid == sender) continue;
            models::Notification n;
            n.id = next_++; n.user_id = uid; n.announcement_id = r.announcement_id; n.title = title; n.content = content; n.created_at = "2026-10-28T10:00:00Z";
            items_.push_back(n);
            r.notifications.push_back(n);
        }
        return r;
    }
    int markRead(int uid, const std::vector<int>& ids) override {
        std::lock_guard<std::mutex> l(m_);
        int n = 0;
        for (auto& x : items_) if (x.user_id == uid && !x.is_read && std::find(ids.begin(), ids.end(), x.id) != ids.end()) { x.is_read = true; ++n; }
        return n;
    }
    int markAllRead(int uid) override {
        std::lock_guard<std::mutex> l(m_);
        int n = 0;
        for (auto& x : items_) if (x.user_id == uid && !x.is_read) { x.is_read = true; ++n; }
        return n;
    }

private:
    InMemoryChatRepository& chat_;
    std::mutex m_;
    std::vector<models::Notification> items_;
    std::map<int, std::string> roles_;
    int next_ = 1, next_announcement_ = 1;
};

} // namespace database
} // namespace uniconnect
