#include "services/ChatService.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <set>
#include <memory>

#include "utils/Logger.h"

namespace uniconnect {
namespace services {

using utils::JsonValue;

namespace {
bool parseId(const std::string& s, int& out) {
    if (s.empty() || s.size() > 9) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    out = std::atoi(s.c_str());
    return out > 0;
}
ServiceResult failure(int status, const std::string& msg) {
    ServiceResult r;
    r.status = status;
    r.error = msg;
    return r;
}
size_t utf8Length(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}
std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}
} // namespace

ServiceResult ChatService::groups(const models::User& caller, const std::string& userText) {
    int userId = caller.id;
    if (!userText.empty()) {
        if (!parseId(userText, userId)) return failure(400, "Invalid user_id.");
        if (userId != caller.id && caller.role != models::UserRole::ADMIN) return failure(403, "You can only see your own groups.");
    }
    auto arr = JsonValue::array();
    for (const auto& g : repo_.groupsForUser(userId)) arr.push_back(g.toJson());
    ServiceResult r;
    r.body = JsonValue::object();
    r.body["groups"] = arr;
    return r;
}

ServiceResult ChatService::messages(const models::User& caller, const std::string& groupText, const std::string& limitText, const std::string& beforeText) {
    int groupId = 0;
    if (!parseId(groupText, groupId)) return failure(400, "Invalid group id.");
    int limit = 50;
    if (!limitText.empty() && (!parseId(limitText, limit) || limit > 200)) return failure(400, "limit must be a number from 1 to 200.");
    std::optional<int> before;
    if (!beforeText.empty()) {
        int b = 0;
        if (!parseId(beforeText, b)) return failure(400, "Invalid before_id.");
        before = b;
    }
    if (!repo_.isMember(groupId, caller.id)) return failure(404, "Group not found.");   // same answer for "does not exist" and "not yours"

    auto page = repo_.messages(groupId, limit + 1, before);       // newest first; one extra row tells us whether there is more
    const bool more = static_cast<int>(page.size()) > limit;
    if (more) page.resize(static_cast<size_t>(limit));
    std::reverse(page.begin(), page.end());                       // oldest first for display

    auto arr = JsonValue::array();
    for (const auto& m : page) arr.push_back(m.toJson());
    ServiceResult r;
    r.body = JsonValue::object();
    r.body["messages"] = arr;
    r.body["has_more"] = more;
    return r;
}

bool ChatService::allowSend(int userId) {
    const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    std::lock_guard<std::mutex> l(rate_mutex_);
    auto& q = sends_[userId];
    while (!q.empty() && now - q.front() > WINDOW_MS) q.pop_front();
    if (static_cast<int>(q.size()) >= MAX_SENDS_PER_WINDOW) return false;
    q.push_back(now);
    if (sends_.size() > 10000) for (auto it = sends_.begin(); it != sends_.end();) it = it->second.empty() ? sends_.erase(it) : std::next(it);
    return true;
}

ServiceResult ChatService::send(const models::User& caller, const std::string& groupText, const JsonValue& body, bool wait) {
    int groupId = 0;
    if (!parseId(groupText, groupId)) return failure(400, "Invalid group id.");
    if (!repo_.isMember(groupId, caller.id)) return failure(404, "Group not found.");

    // The sender is always the signed-in user; a request that names someone else is refused.
    if (body.contains("sender_id")) {
        int claimed = 0;
        if (!parseId(body["sender_id"].asString(), claimed) || claimed != caller.id) return failure(403, "You can only send messages as yourself.");
    }
    const std::string content = trim(body["content"].asString());
    if (content.empty()) return failure(400, "Message cannot be empty.");
    if (utf8Length(content) > static_cast<size_t>(MAX_MESSAGE_CHARS)) return failure(400, "Message is too long (limit " + std::to_string(MAX_MESSAGE_CHARS) + " characters).");
    if (content.find('\0') != std::string::npos) return failure(400, "Message contains invalid characters.");
    // `priority` is a scheduling hint for the server's task queue; it is validated here but not stored.
    if (body.contains("priority")) {
        const int p = body["priority"].asInt(-1);
        if (p < 1 || p > 10) return failure(400, "priority must be a number from 1 to 10.");
    }
    if (!allowSend(caller.id)) return failure(429, "You are sending messages too quickly. Please wait a moment.");

    const int priority = body.contains("priority") ? body["priority"].asInt(1) : 1;

    if (!dispatcher_) {   // no scheduler configured: store and deliver right here
        const auto msg = repo_.addMessage(groupId, caller.id, content);
        if (!msg) return failure(500, "Could not save the message.");
        if (listener_) listener_(*msg, repo_.memberIds(groupId));
        ServiceResult r;
        r.body = JsonValue::object();
        r.body["success"] = true;
        r.body["message"] = msg->toJson();
        return r;
    }

    // Scheduled path: the message becomes a task with a priority, ordered by the scheduler and run by a worker thread.
    auto promise = std::make_shared<std::promise<std::optional<models::ChatMessage>>>();
    auto future = promise->get_future();
    const int senderId = caller.id;
    const bool queued = dispatcher_->submit("CHAT_MESSAGE_" + std::to_string(groupId), priority, 5, [this, groupId, senderId, content, promise]() {
        std::optional<models::ChatMessage> msg;
        try {
            msg = repo_.addMessage(groupId, senderId, content);
            if (msg && listener_) listener_(*msg, repo_.memberIds(groupId));
        } catch (const std::exception& e) {
            utils::Logger::getInstance().error("Chat", "Could not deliver a message:", e.what());
        }
        promise->set_value(msg);
    });
    if (!queued) return failure(503, "The server is busy. Please try again in a moment.");

    ServiceResult r;
    r.body = JsonValue::object();
    if (!wait) {
        r.status = 202;
        r.body["success"] = true;
        r.body["queued"] = true;
        return r;
    }
    if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) return failure(503, "The server is busy. Please try again in a moment.");
    const auto msg = future.get();
    if (!msg) return failure(500, "Could not save the message.");
    r.body["success"] = true;
    r.body["message"] = msg->toJson();
    return r;
}

ServiceResult ChatService::directory(const models::User& caller, const std::string& query) {
    if (utf8Length(query) > 50) return failure(400, "Search text is too long.");
    auto arr = JsonValue::array();
    for (const auto& u : repo_.searchUsers(caller.id, trim(query), 50)) {
        auto o = JsonValue::object();
        o["user_id"] = u.id;
        o["name"] = u.name;
        o["role"] = u.role;
        o["online"] = online_ ? online_(u.id) : false;
        arr.push_back(o);
    }
    ServiceResult r;
    r.body = JsonValue::object();
    r.body["users"] = arr;
    return r;
}

ServiceResult ChatService::startDirect(const models::User& caller, const JsonValue& body) {
    int other = 0;
    if (!parseId(body["user_id"].asString(), other)) return failure(400, "A valid user_id is required.");
    if (other == caller.id) return failure(400, "You cannot start a chat with yourself.");
    if (!repo_.usersExist({other})) return failure(404, "Person not found.");
    const auto group = repo_.findOrCreateDirect(caller.id, other);
    if (!group) return failure(500, "Could not start the chat.");
    if (group_added_) if (const auto theirs = repo_.groupFor(group->id, other)) group_added_(other, *theirs);   // it appears in their list at once
    ServiceResult r;
    r.body = JsonValue::object();
    r.body["group"] = group->toJson();
    return r;
}

ServiceResult ChatService::createGroup(const models::User& caller, const JsonValue& body) {
    const std::string name = trim(body["name"].asString());
    if (name.empty()) return failure(400, "Give the group a name.");
    if (utf8Length(name) > 60) return failure(400, "The group name is too long (limit 60 characters).");
    for (unsigned char c : name) if (c < 0x20 || c == 0x7f) return failure(400, "The group name contains invalid characters.");
    if (!body["member_ids"].isArray() || body["member_ids"].size() == 0) return failure(400, "Choose at least one person.");
    if (body["member_ids"].size() > 49) return failure(400, "A group can have at most 50 people.");
    std::set<int> ids;
    for (size_t i = 0; i < body["member_ids"].size(); ++i) {
        int id = 0;
        if (!parseId(body["member_ids"].at(i).asString(), id)) return failure(400, "member_ids must be a list of user ids.");
        if (id != caller.id) ids.insert(id);
    }
    if (ids.empty()) return failure(400, "Choose at least one person other than yourself.");
    const std::vector<int> members(ids.begin(), ids.end());
    if (!repo_.usersExist(members)) return failure(404, "Someone on the list was not found.");
    const auto group = repo_.createGroup(caller.id, name, members);
    if (!group) return failure(500, "Could not create the group.");
    if (group_added_) for (int id : members) if (const auto theirs = repo_.groupFor(group->id, id)) group_added_(id, *theirs);
    ServiceResult r;
    r.status = 201;
    r.body = JsonValue::object();
    r.body["group"] = group->toJson();
    return r;
}

ServiceResult ChatService::leave(const models::User& caller, const std::string& groupText) {
    int groupId = 0;
    if (!parseId(groupText, groupId)) return failure(400, "Invalid group id.");
    if (!repo_.isMember(groupId, caller.id)) return failure(404, "Group not found.");
    if (repo_.groupType(groupId) != "GROUP") return failure(400, "You can only leave groups that people created.");
    if (!repo_.leaveGroup(groupId, caller.id)) return failure(500, "Could not leave the group.");
    ServiceResult r;
    r.body = JsonValue::object();
    r.body["success"] = true;
    return r;
}

ServiceResult ChatService::mute(const models::User& caller, const std::string& groupText, const JsonValue& body) {
    int groupId = 0;
    if (!parseId(groupText, groupId)) return failure(400, "Invalid group id.");
    if (!body.contains("muted")) return failure(400, "Send {\"muted\": true} or {\"muted\": false}.");
    if (!repo_.isMember(groupId, caller.id)) return failure(404, "Group not found.");
    const bool muted = body["muted"].asBool(false);
    repo_.setMuted(groupId, caller.id, muted);
    ServiceResult r;
    r.body = JsonValue::object();
    r.body["success"] = true;
    r.body["muted"] = muted;
    return r;
}

ServiceResult ChatService::markRead(const models::User& caller, const std::string& groupText, const JsonValue& body) {
    int groupId = 0;
    if (!parseId(groupText, groupId)) return failure(400, "Invalid group id.");
    if (!repo_.isMember(groupId, caller.id)) return failure(404, "Group not found.");
    std::optional<int> upTo;
    if (body.contains("message_id")) {
        int id = 0;
        if (!parseId(body["message_id"].asString(), id)) return failure(400, "Invalid message_id.");
        upTo = id;
    }
    ServiceResult r;
    r.body = JsonValue::object();
    r.body["success"] = true;
    r.body["last_read_message_id"] = repo_.markRead(groupId, caller.id, upTo);
    return r;
}

} // namespace services
} // namespace uniconnect
