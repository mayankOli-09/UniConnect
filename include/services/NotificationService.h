#pragma once
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "concurrency/Dispatcher.h"
#include "database/ChatRepository.h"
#include "database/NotificationRepository.h"
#include "models/User.h"
#include "services/ServiceResult.h"

namespace uniconnect {
namespace services {

class NotificationService {
public:
    // Called after an announcement is stored, with every notification that was created (used for live push).
    using NotifyListener = std::function<void(const std::vector<models::Notification>&)>;

    NotificationService(database::NotificationRepository& repo, database::ChatRepository& chat) : repo_(repo), chat_(chat) {}
    void setDispatcher(concurrency::Dispatcher* d) { dispatcher_ = d; }
    void setListener(NotifyListener l) { listener_ = std::move(l); }

    ServiceResult list(const models::User& caller, const std::string& user_id);
    ServiceResult announce(const models::User& caller, const utils::JsonValue& body);
    ServiceResult notice(const models::User& caller, const utils::JsonValue& body);     // admin only: {audience, title, content}
    ServiceResult markRead(const models::User& caller, const utils::JsonValue& body);

    static constexpr int MAX_TITLE_CHARS = 200;
    static constexpr int MAX_CONTENT_CHARS = 4000;
    static constexpr int MAX_ANNOUNCEMENTS_PER_MINUTE = 10;

private:
    bool allowAnnounce(int user_id);

    database::NotificationRepository& repo_;
    database::ChatRepository& chat_;
    concurrency::Dispatcher* dispatcher_ = nullptr;
    NotifyListener listener_;
    std::mutex rate_mutex_;
    std::map<int, std::deque<long long>> posts_;
};

} // namespace services
} // namespace uniconnect
