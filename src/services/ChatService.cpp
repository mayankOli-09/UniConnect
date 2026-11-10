#include "services/ChatService.h"
#include "websocket/WebSocket.h"
#include "utils/UUID.h"
#include "utils/Logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace uniconnect {
namespace services {

ChatService::ChatService(database::MessageRepository& msg_repo,
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

bool ChatService::postMessage(const std::string& group_id,
                              const std::string& sender_id,
                              const std::string& sender_name,
                              const std::string& content,
                              models::MessageType type,
                              int priority) {

    models::Message msg;
    msg.id = "msg-" + utils::UUID::generate().substr(0, 8);
    msg.group_id = group_id;
    msg.sender_id = sender_id;
    msg.sender_name = sender_name;
    msg.content = content;
    msg.type = type;
    msg.priority = priority;
    msg.timestamp = currentTimestamp();

    // Package into an OS Concurrency Task for asynchronous dispatching & scheduling
    concurrency::Task task(
        msg.id,
        "DISPATCH_MESSAGE_" + group_id,
        [this, msg]() {
            // 1. Persist to Database
            msg_repo_.saveMessage(msg);

            // 2. Broadcast to WebSocket subscribers in group
            auto ws_payload = utils::JsonValue::object();
            ws_payload["event"] = "new_message";
            ws_payload["data"] = msg.toJson();

            websocket::WebSocketManager::getInstance().broadcastToGroup(
                msg.group_id,
                ws_payload.serialize(false)
            );

            utils::Logger::getInstance().info("ChatService", "Dispatched message", msg.id, "to group", msg.group_id);
        },
        priority, // Used by Scheduler (Priority policy)
        5         // Estimated burst ms (Used by SJF policy)
    );

    if (thread_pool_) {
        thread_pool_->submit(std::move(task));
    } else {
        // Direct execution fallback if thread pool not initialized
        task.work();
    }

    return true;
}

std::vector<models::Message> ChatService::getMessages(const std::string& group_id, size_t limit) {
    return msg_repo_.getMessagesByGroup(group_id, limit);
}

std::vector<models::CommunicationGroup> ChatService::getUserGroups(const std::string& user_id) {
    return enroll_repo_.getUserGroups(user_id);
}

} // namespace services
} // namespace uniconnect
