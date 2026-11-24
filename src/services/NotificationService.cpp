#include "services/NotificationService.h"

#include <chrono>
#include <future>
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

ServiceResult NotificationService::list(const models::User& caller, const std::string& userText) {
    int userId = caller.id;
    if (!userText.empty()) {
        if (!parseId(userText, userId)) return failure(400, "Invalid user_id.");
        if (userId != caller.id && caller.role != models::UserRole::ADMIN) return failure(403, "You can only see your own notifications.");
    }
    auto arr = JsonValue::array();
    for (const auto& n : repo_.forUser(userId, 100)) arr.push_back(n.toJson());
    ServiceResult r;
    r.body = JsonValue::object();
    r.body["notifications"] = arr;
    r.body["unread_count"] = repo_.unreadCount(userId);
    return r;
}

bool NotificationService::allowAnnounce(int userId) {
    const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    std::lock_guard<std::mutex> l(rate_mutex_);
    auto& q = posts_[userId];
    while (!q.empty() && now - q.front() > 60000) q.pop_front();
    if (static_cast<int>(q.size()) >= MAX_ANNOUNCEMENTS_PER_MINUTE) return false;
    q.push_back(now);
    return true;
}

ServiceResult NotificationService::announce(const models::User& caller, const JsonValue& body) {
    if (caller.role != models::UserRole::FACULTY && caller.role != models::UserRole::ADMIN)
        return failure(403, "Only faculty and administrators can post announcements.");

    int groupId = 0;
    if (!parseId(body["group_id"].asString(), groupId)) return failure(400, "A valid group_id is required.");
    // Faculty can announce to groups they belong to; admins to any group. Others get the same answer as for a missing group.
    const bool allowed = caller.role == models::UserRole::ADMIN ? repo_.groupExists(groupId) : chat_.isMember(groupId, caller.id);
    if (!allowed) return failure(404, "Group not found.");

    const std::string title = trim(body["title"].asString());
    const std::string content = trim(body["content"].asString());
    if (title.empty()) return failure(400, "Title is required.");
    if (content.empty()) return failure(400, "Content is required.");
    if (utf8Length(title) > static_cast<size_t>(MAX_TITLE_CHARS)) return failure(400, "Title is too long (limit " + std::to_string(MAX_TITLE_CHARS) + " characters).");
    if (utf8Length(content) > static_cast<size_t>(MAX_CONTENT_CHARS)) return failure(400, "Content is too long (limit " + std::to_string(MAX_CONTENT_CHARS) + " characters).");
    int priority = 5;                                  // a scheduling hint: announcements default to "medium"
    if (body.contains("priority")) {
        priority = body["priority"].asInt(-1);
        if (priority < 1 || priority > 10) return failure(400, "priority must be a number from 1 to 10.");
    }
    if (!allowAnnounce(caller.id)) return failure(429, "You are posting announcements too quickly. Please wait a moment.");

    auto work = [this, senderId = caller.id, groupId, title, content]() -> std::optional<database::AnnouncementResult> {
        auto res = repo_.createAnnouncement(senderId, groupId, title, content);
        if (res && listener_) listener_(res->notifications);
        return res;
    };

    std::optional<database::AnnouncementResult> result;
    if (!dispatcher_) {
        result = work();
    } else {
        auto promise = std::make_shared<std::promise<std::optional<database::AnnouncementResult>>>();
        auto future = promise->get_future();
        const bool queued = dispatcher_->submit("ANNOUNCEMENT_" + std::to_string(groupId), priority, 15, [work, promise]() {
            std::optional<database::AnnouncementResult> res;
            try { res = work(); } catch (const std::exception& e) { utils::Logger::getInstance().error("Announce", "Failed:", e.what()); }
            promise->set_value(res);
        });
        if (!queued) return failure(503, "The server is busy. Please try again in a moment.");
        if (future.wait_for(std::chrono::seconds(10)) != std::future_status::ready) return failure(503, "The server is busy. Please try again in a moment.");
        result = future.get();
    }
    if (!result) return failure(500, "Could not save the announcement.");

    ServiceResult r;
    r.body = JsonValue::object();
    r.body["success"] = true;
    r.body["announcement_id"] = result->announcement_id;
    r.body["recipients"] = result->notifications.size();
    r.body["message"] = "Announcement sent to " + std::to_string(result->notifications.size()) + (result->notifications.size() == 1 ? " person." : " people.");
    return r;
}

ServiceResult NotificationService::notice(const models::User& caller, const JsonValue& body) {
    if (caller.role != models::UserRole::ADMIN) return failure(403, "Only administrators can send notices.");
    std::string audience = body["audience"].asString();
    for (auto& c : audience) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (audience != "STUDENTS" && audience != "FACULTY" && audience != "BOTH") return failure(400, "audience must be STUDENTS, FACULTY or BOTH.");
    const std::string title = trim(body["title"].asString());
    const std::string content = trim(body["content"].asString());
    if (title.empty()) return failure(400, "Title is required.");
    if (content.empty()) return failure(400, "Content is required.");
    if (utf8Length(title) > static_cast<size_t>(MAX_TITLE_CHARS)) return failure(400, "Title is too long (limit " + std::to_string(MAX_TITLE_CHARS) + " characters).");
    if (utf8Length(content) > static_cast<size_t>(MAX_CONTENT_CHARS)) return failure(400, "Content is too long (limit " + std::to_string(MAX_CONTENT_CHARS) + " characters).");
    if (!allowAnnounce(caller.id)) return failure(429, "You are posting too quickly. Please wait a moment.");

    auto work = [this, senderId = caller.id, audience, title, content]() -> std::optional<database::AnnouncementResult> {
        auto res = repo_.createNotice(senderId, audience, title, content);
        if (res && listener_) listener_(res->notifications);       // live pop-up for everyone who is online
        return res;
    };
    std::optional<database::AnnouncementResult> result;
    if (!dispatcher_) {
        result = work();
    } else {
        auto promise = std::make_shared<std::promise<std::optional<database::AnnouncementResult>>>();
        auto future = promise->get_future();
        const bool queued = dispatcher_->submit("NOTICE_" + audience, 7, 20, [work, promise]() {
            std::optional<database::AnnouncementResult> res;
            try { res = work(); } catch (const std::exception& e) { utils::Logger::getInstance().error("Notice", "Failed:", e.what()); }
            promise->set_value(res);
        });
        if (!queued) return failure(503, "The server is busy. Please try again in a moment.");
        if (future.wait_for(std::chrono::seconds(10)) != std::future_status::ready) return failure(503, "The server is busy. Please try again in a moment.");
        result = future.get();
    }
    if (!result) return failure(500, "Could not save the notice.");

    ServiceResult r;
    r.body = JsonValue::object();
    r.body["success"] = true;
    r.body["announcement_id"] = result->announcement_id;
    r.body["recipients"] = result->notifications.size();
    r.body["message"] = "Notice sent to " + std::to_string(result->notifications.size()) + (result->notifications.size() == 1 ? " person." : " people.");
    return r;
}

ServiceResult NotificationService::markRead(const models::User& caller, const JsonValue& body) {
    int changed = 0;
    if (body["all"].asBool(false)) {
        changed = repo_.markAllRead(caller.id);
    } else if (body["ids"].isArray() && body["ids"].size() > 0) {
        if (body["ids"].size() > 200) return failure(400, "At most 200 notifications can be marked at once.");
        std::vector<int> ids;
        for (size_t i = 0; i < body["ids"].size(); ++i) {
            int id = 0;
            if (!parseId(body["ids"].at(i).asString(), id)) return failure(400, "ids must be a list of notification ids.");
            ids.push_back(id);
        }
        changed = repo_.markRead(caller.id, ids);
    } else {
        return failure(400, "Send {\"ids\": [...]} or {\"all\": true}.");
    }
    ServiceResult r;
    r.body = JsonValue::object();
    r.body["success"] = true;
    r.body["updated"] = changed;
    r.body["unread_count"] = repo_.unreadCount(caller.id);
    return r;
}

} // namespace services
} // namespace uniconnect
