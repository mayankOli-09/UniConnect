#include "database/MessageRepository.h"
#include <algorithm>

namespace uniconnect {
namespace database {

MessageRepository::MessageRepository(Database& db) : db_(db) {}

bool MessageRepository::saveMessage(const models::Message& message) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    db_.messages.push_back(message);
    return true;
}

std::vector<models::Message> MessageRepository::getMessagesByGroup(const std::string& group_id, size_t limit) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::Message> list;
    for (auto it = db_.messages.rbegin(); it != db_.messages.rend(); ++it) {
        if (it->group_id == group_id) {
            list.push_back(*it);
            if (list.size() >= limit) break;
        }
    }
    std::reverse(list.begin(), list.end());
    return list;
}

std::optional<models::Message> MessageRepository::findMessageById(const std::string& id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    for (const auto& m : db_.messages) {
        if (m.id == id) return m;
    }
    return std::nullopt;
}

bool MessageRepository::saveNotification(const models::Notification& notif) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    db_.notifications.push_back(notif);
    return true;
}

std::vector<models::Notification> MessageRepository::getNotificationsByUser(const std::string& user_id, bool unread_only) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::Notification> list;
    for (auto it = db_.notifications.rbegin(); it != db_.notifications.rend(); ++it) {
        if (it->user_id == user_id) {
            if (!unread_only || !it->is_read) {
                list.push_back(*it);
            }
        }
    }
    return list;
}

bool MessageRepository::markNotificationAsRead(const std::string& notification_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    for (auto& n : db_.notifications) {
        if (n.id == notification_id) {
            n.is_read = true;
            return true;
        }
    }
    return false;
}

} // namespace database
} // namespace uniconnect
