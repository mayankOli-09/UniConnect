#pragma once

#include <string>
#include <vector>
#include <memory>
#include "database/MessageRepository.h"
#include "database/EnrollmentRepository.h"
#include "concurrency/ThreadPool.h"
#include "models/Message.h"
#include "utils/Json.h"

namespace uniconnect {
namespace services {

class ChatService {
public:
    ChatService(database::MessageRepository& msg_repo,
                database::EnrollmentRepository& enroll_repo,
                std::shared_ptr<concurrency::ThreadPool> thread_pool);

    // Sends a message: processes via Concurrency Engine & broadcasts via WebSocket
    bool postMessage(const std::string& group_id,
                     const std::string& sender_id,
                     const std::string& sender_name,
                     const std::string& content,
                     models::MessageType type = models::MessageType::TEXT,
                     int priority = 1);

    std::vector<models::Message> getMessages(const std::string& group_id, size_t limit = 50);
    std::vector<models::CommunicationGroup> getUserGroups(const std::string& user_id);

private:
    database::MessageRepository& msg_repo_;
    database::EnrollmentRepository& enroll_repo_;
    std::shared_ptr<concurrency::ThreadPool> thread_pool_;
};

} // namespace services
} // namespace uniconnect
