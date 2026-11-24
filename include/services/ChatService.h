#pragma once
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "concurrency/Dispatcher.h"
#include "database/ChatRepository.h"
#include "models/User.h"
#include "services/ServiceResult.h"

namespace uniconnect {
namespace services {

class ChatService {
public:
    // Called after a message is stored, with the user ids that should receive it (used for real-time delivery).
    using MessageListener = std::function<void(const models::ChatMessage&, const std::vector<int>& recipients)>;

    explicit ChatService(database::ChatRepository& repo) : repo_(repo) {}
    void setMessageListener(MessageListener l) { listener_ = std::move(l); }
    // With a dispatcher, storing and delivering a message becomes a scheduled task; without one it runs inline.
    void setDispatcher(concurrency::Dispatcher* d) { dispatcher_ = d; }
    bool isMember(int user_id, int group_id) { return repo_.isMember(group_id, user_id); }
    std::vector<int> members(int group_id) { return repo_.memberIds(group_id); }

    // Called when someone is added to a chat they did not start (a new direct chat or group), with that person's view of it.
    using GroupAddedListener = std::function<void(int user_id, const models::ChatGroup&)>;
    void setGroupAddedListener(GroupAddedListener l) { group_added_ = std::move(l); }
    void setOnlineChecker(std::function<bool(int)> f) { online_ = std::move(f); }

    ServiceResult directory(const models::User& caller, const std::string& query);                       // people you can chat with
    ServiceResult startDirect(const models::User& caller, const utils::JsonValue& body);                 // {user_id}
    ServiceResult createGroup(const models::User& caller, const utils::JsonValue& body);                 // {name, member_ids}
    ServiceResult leave(const models::User& caller, const std::string& group_id);
    ServiceResult mute(const models::User& caller, const std::string& group_id, const utils::JsonValue& body);        // {muted}
    ServiceResult markRead(const models::User& caller, const std::string& group_id, const utils::JsonValue& body);    // {message_id?}

    ServiceResult groups(const models::User& caller, const std::string& user_id);
    ServiceResult messages(const models::User& caller, const std::string& group_id, const std::string& limit, const std::string& before_id);
    // wait = true: returns the stored message (REST). wait = false: returns 202 once queued; delivery arrives via the listener (WebSocket).
    ServiceResult send(const models::User& caller, const std::string& group_id, const utils::JsonValue& body, bool wait = true);

    static constexpr int MAX_MESSAGE_CHARS = 4000;
    static constexpr int MAX_SENDS_PER_WINDOW = 30;     // per user ...
    static constexpr int WINDOW_MS = 10000;             // ... per 10 seconds

private:
    bool allowSend(int user_id);

    database::ChatRepository& repo_;
    MessageListener listener_;
    concurrency::Dispatcher* dispatcher_ = nullptr;
    GroupAddedListener group_added_;
    std::function<bool(int)> online_;
    std::mutex rate_mutex_;
    std::map<int, std::deque<long long>> sends_;
};

} // namespace services
} // namespace uniconnect
