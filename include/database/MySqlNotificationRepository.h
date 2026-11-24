#pragma once
#include "database/MySqlDatabase.h"
#include "database/NotificationRepository.h"

namespace uniconnect {
namespace database {

class MySqlNotificationRepository : public NotificationRepository {
public:
    explicit MySqlNotificationRepository(MySqlDatabase& db) : db_(db) {}
    std::vector<models::Notification> forUser(int user_id, int limit) override;
    int unreadCount(int user_id) override;
    bool groupExists(int group_id) override;
    std::optional<AnnouncementResult> createAnnouncement(int sender_id, int group_id, const std::string& title, const std::string& content) override;
    std::optional<AnnouncementResult> createNotice(int sender_id, const std::string& audience, const std::string& title, const std::string& content) override;
    std::vector<models::Notification> notifyUsers(const std::vector<int>& user_ids, const std::string& kind, const std::string& title, const std::string& content) override;
    int markRead(int user_id, const std::vector<int>& notification_ids) override;
    int markAllRead(int user_id) override;

private:
    MySqlDatabase& db_;
};

} // namespace database
} // namespace uniconnect
