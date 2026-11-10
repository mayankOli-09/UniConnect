#include "services/NotificationService.h"
#include "websocket/WebSocket.h"
#include "utils/UUID.h"
#include "utils/Logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace uniconnect {
namespace services {

NotificationService::NotificationService(database::MessageRepository& msg_repo,
                                         database::EnrollmentRepository& enroll_repo,
                                         std::shared_ptr<concurrency::ThreadPool> thread_pool)
    : msg_repo_(msg_repo),
      enroll_repo_(enroll_repo),
      thread_pool_(thread_pool) {}

static std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt{};
#if defined(_WIN32)
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif
    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool NotificationService::sendAnnouncement(const std::string& group_id,
                                           const std::string& title,
                                           const std::string& message_text,
                                           int priority) {

    auto members = enroll_repo_.getGroupMembers(group_id);
    std::string notif_time = currentTimestamp();

    // High-priority task dispatched to Concurrency ThreadPool
    concurrency::Task task(
        "notif-fanout-" + utils::UUID::generate().substr(0, 8),
        "NOTIFICATION_FANOUT_" + group_id,
        [this, members, title, message_text, priority, notif_time]() {
            for (const auto& uid : members) {
                models::Notification notif;
                notif.id = "notif-" + utils::UUID::generate().substr(0, 8);
                notif.user_id = uid;
                notif.title = title;
                notif.message = message_text;
                notif.is_read = false;
                notif.priority = priority;
                notif.timestamp = notif_time;

                msg_repo_.saveNotification(notif);

                // Push real-time notification alert via WebSocket to this specific user
                auto ws_payload = utils::JsonValue::object();
                ws_payload["event"] = "notification";
                ws_payload["data"] = notif.toJson();

                websocket::WebSocketManager::getInstance().broadcastToUser(
                    uid,
                    ws_payload.serialize(false)
                );
            }
            utils::Logger::getInstance().info("NotificationService", "Broadcasted notification to", members.size(), "members.");
        },
        priority, // Higher priority than general chat
        15        // Estimated burst time
    );

    if (thread_pool_) {
        thread_pool_->submit(std::move(task));
    } else {
        task.work();
    }

    return true;
}

std::vector<models::Notification> NotificationService::getUserNotifications(const std::string& user_id, bool unread_only) {
    return msg_repo_.getNotificationsByUser(user_id, unread_only);
}

bool NotificationService::markAsRead(const std::string& notif_id) {
    return msg_repo_.markNotificationAsRead(notif_id);
}

} // namespace services
} // namespace uniconnect
