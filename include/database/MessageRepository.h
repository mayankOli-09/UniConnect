#pragma once

#include <string>
#include <vector>
#include <optional>
#include "models/Message.h"
#include "models/Notification.h"
#include "database/Database.h"

namespace uniconnect {
namespace database {

class MessageRepository {
public:
    explicit MessageRepository(Database& db = Database::getInstance());

    bool saveMessage(const models::Message& message);
    std::vector<models::Message> getMessagesByGroup(const std::string& group_id, size_t limit = 50);
    std::optional<models::Message> findMessageById(const std::string& id);

    bool saveNotification(const models::Notification& notif);
    std::vector<models::Notification> getNotificationsByUser(const std::string& user_id, bool unread_only = false);
    bool markNotificationAsRead(const std::string& notification_id);

private:
    Database& db_;
};

} // namespace database
} // namespace uniconnect
