#pragma once

#include <string>
#include <vector>
#include <memory>
#include "database/MessageRepository.h"
#include "database/EnrollmentRepository.h"
#include "concurrency/ThreadPool.h"
#include "models/Notification.h"
#include "utils/Json.h"

namespace uniconnect {
namespace services {

class NotificationService {
public:
    NotificationService(database::MessageRepository& msg_repo,
                        database::EnrollmentRepository& enroll_repo,
                        std::shared_ptr<concurrency::ThreadPool> thread_pool);

    // Broadcast an announcement or urgent alert to all group members
    bool sendAnnouncement(const std::string& group_id,
                          const std::string& title,
                          const std::string& message_text,
                          int priority = 5);

    std::vector<models::Notification> getUserNotifications(const std::string& user_id, bool unread_only = false);
    bool markAsRead(const std::string& notif_id);

private:
    database::MessageRepository& msg_repo_;
    database::EnrollmentRepository& enroll_repo_;
    std::shared_ptr<concurrency::ThreadPool> thread_pool_;
};

} // namespace services
} // namespace uniconnect
