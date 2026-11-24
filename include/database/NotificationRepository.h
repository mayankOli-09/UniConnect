#pragma once
#include <optional>
#include <vector>

#include "models/Notification.h"

namespace uniconnect {
namespace database {

struct AnnouncementResult {
    int announcement_id = 0;
    std::vector<models::Notification> notifications;   // one per recipient (the sender is not notified)
};

class NotificationRepository {
public:
    virtual ~NotificationRepository() = default;
    virtual std::vector<models::Notification> forUser(int user_id, int limit) = 0;   // newest first
    virtual int unreadCount(int user_id) = 0;
    virtual bool groupExists(int group_id) = 0;
    // Stores the announcement and one notification per group member, atomically.
    virtual std::optional<AnnouncementResult> createAnnouncement(int sender_id, int group_id, const std::string& title, const std::string& content) = 0;
    // An admin notice: stored once and copied to every user with a matching role (audience: STUDENTS, FACULTY or BOTH). The sender is not notified.
    virtual std::optional<AnnouncementResult> createNotice(int sender_id, const std::string& audience, const std::string& title, const std::string& content) = 0;
    // One notification each for the given people (kind: ANNOUNCEMENT, NOTICE or MENTION), stored together.
    virtual std::vector<models::Notification> notifyUsers(const std::vector<int>& user_ids, const std::string& kind, const std::string& title, const std::string& content) = 0;
    virtual int markRead(int user_id, const std::vector<int>& notification_ids) = 0;  // only the user's own rows; returns how many changed
    virtual int markAllRead(int user_id) = 0;
};

} // namespace database
} // namespace uniconnect
