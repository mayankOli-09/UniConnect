#include "database/MySqlNotificationRepository.h"

#include <set>
#include <stdexcept>

namespace uniconnect {
namespace database {

namespace {
const char* kColumns =
    "notification_id, user_id, announcement_id, title, content, kind, is_read, DATE_FORMAT(created_at, '%Y-%m-%dT%H:%i:%sZ') AS created_at ";

models::Notification toNotification(const DbRow& r) {
    models::Notification n;
    n.id = rowInt(r, "notification_id");
    n.user_id = rowInt(r, "user_id");
    n.announcement_id = rowInt(r, "announcement_id");
    n.title = rowStr(r, "title");
    n.content = rowStr(r, "content");
    n.kind = rowStr(r, "kind", "ANNOUNCEMENT");
    n.is_read = rowInt(r, "is_read") != 0;
    n.created_at = rowStr(r, "created_at");
    return n;
}
} // namespace

std::vector<models::Notification> MySqlNotificationRepository::forUser(int userId, int limit) {
    const auto r = db_.execute(std::string("SELECT ") + kColumns + "FROM notifications WHERE user_id = ? ORDER BY created_at DESC, notification_id DESC LIMIT ?",
                               {std::to_string(userId), std::to_string(limit)});
    if (!r.ok) throw std::runtime_error("Could not read the notifications");
    std::vector<models::Notification> out;
    for (const auto& row : r.rows) out.push_back(toNotification(row));
    return out;
}

int MySqlNotificationRepository::unreadCount(int userId) {
    const auto r = db_.execute("SELECT COUNT(*) AS n FROM notifications WHERE user_id = ? AND is_read = 0", {std::to_string(userId)});
    return r.ok && !r.rows.empty() ? rowInt(r.rows[0], "n") : 0;
}

bool MySqlNotificationRepository::groupExists(int groupId) {
    const auto r = db_.execute("SELECT 1 AS present FROM conversations WHERE conversation_id = ?", {std::to_string(groupId)});
    return r.ok && !r.rows.empty();
}

std::optional<AnnouncementResult> MySqlNotificationRepository::createAnnouncement(int senderId, int groupId, const std::string& title, const std::string& content) {
    MySqlDatabase::Transaction tx(db_);
    if (!tx.ok()) return std::nullopt;

    // The announcement is scoped like the group it is posted to (course, section or department).
    const auto conv = tx.execute("SELECT course_id, section_id, department_id FROM conversations WHERE conversation_id = ?", {std::to_string(groupId)});
    if (!conv.ok || conv.rows.empty()) return std::nullopt;
    const DbRow& c = conv.rows[0];
    auto scope = [&](const char* key) -> DbValue { auto it = c.find(key); return it == c.end() ? DbValue{} : it->second; };

    const auto ann = tx.execute("INSERT INTO announcements (title, content, created_by, course_id, section_id, department_id) VALUES (?, ?, ?, ?, ?, ?)",
                                {title, content, std::to_string(senderId), scope("course_id"), scope("section_id"), scope("department_id")});
    if (!ann.ok) return std::nullopt;
    const std::string annId = std::to_string(ann.insert_id);

    // One notification per member, except the sender.
    if (!tx.execute("INSERT INTO notifications (user_id, announcement_id, title, content) "
                    "SELECT m.user_id, ?, ?, ? FROM conversation_members m WHERE m.conversation_id = ? AND m.user_id <> ?",
                    {annId, title, content, std::to_string(groupId), std::to_string(senderId)}).ok) return std::nullopt;

    const auto made = tx.execute(std::string("SELECT ") + kColumns + "FROM notifications WHERE announcement_id = ? ORDER BY notification_id", {annId});
    if (!made.ok) return std::nullopt;
    AnnouncementResult out;
    out.announcement_id = static_cast<int>(ann.insert_id);
    for (const auto& row : made.rows) out.notifications.push_back(toNotification(row));
    if (!tx.commit()) return std::nullopt;
    return out;
}

std::optional<AnnouncementResult> MySqlNotificationRepository::createNotice(int senderId, const std::string& audience, const std::string& title, const std::string& content) {
    std::string roles;                                   // only fixed values reach the SQL text, never the caller's own words
    if (audience == "STUDENTS") roles = "'STUDENT'";
    else if (audience == "FACULTY") roles = "'FACULTY'";
    else if (audience == "BOTH") roles = "'STUDENT', 'FACULTY'";
    else return std::nullopt;

    MySqlDatabase::Transaction tx(db_);
    if (!tx.ok()) return std::nullopt;
    const auto ann = tx.execute("INSERT INTO announcements (title, content, created_by) VALUES (?, ?, ?)", {title, content, std::to_string(senderId)});
    if (!ann.ok) return std::nullopt;
    const std::string annId = std::to_string(ann.insert_id);
    if (!tx.execute("INSERT INTO notifications (user_id, announcement_id, title, content, kind) "
                    "SELECT user_id, ?, ?, ?, 'NOTICE' FROM users WHERE user_id <> ? AND role IN (" + roles + ")",
                    {annId, title, content, std::to_string(senderId)}).ok) return std::nullopt;
    const auto made = tx.execute(std::string("SELECT ") + kColumns + "FROM notifications WHERE announcement_id = ? ORDER BY notification_id", {annId});
    if (!made.ok) return std::nullopt;
    AnnouncementResult out;
    out.announcement_id = static_cast<int>(ann.insert_id);
    for (const auto& row : made.rows) out.notifications.push_back(toNotification(row));
    if (!tx.commit()) return std::nullopt;
    return out;
}

std::vector<models::Notification> MySqlNotificationRepository::notifyUsers(const std::vector<int>& userIds, const std::string& kind, const std::string& title, const std::string& content) {
    std::vector<models::Notification> out;
    const std::set<int> unique(userIds.begin(), userIds.end());
    if (unique.empty() || unique.size() > 200) return out;
    if (kind != "ANNOUNCEMENT" && kind != "NOTICE" && kind != "MENTION") return out;

    MySqlDatabase::Transaction tx(db_);
    if (!tx.ok()) return out;
    std::vector<DbValue> ids;
    for (int uid : unique) {
        const auto r = tx.execute("INSERT INTO notifications (user_id, title, content, kind) VALUES (?, ?, ?, ?)", {std::to_string(uid), title, content, kind});
        if (!r.ok) return {};
        ids.push_back(std::to_string(r.insert_id));
    }
    std::string marks;
    for (size_t i = 0; i < ids.size(); ++i) marks += i ? ", ?" : "?";
    const auto made = tx.execute(std::string("SELECT ") + kColumns + "FROM notifications WHERE notification_id IN (" + marks + ") ORDER BY notification_id", ids);
    if (!made.ok || !tx.commit()) return {};
    for (const auto& row : made.rows) out.push_back(toNotification(row));
    return out;
}

int MySqlNotificationRepository::markRead(int userId, const std::vector<int>& ids) {
    if (ids.empty()) return 0;
    std::string sql = "UPDATE notifications SET is_read = 1 WHERE user_id = ? AND is_read = 0 AND notification_id IN (";
    std::vector<DbValue> params = {std::to_string(userId)};
    for (size_t i = 0; i < ids.size(); ++i) { sql += i ? ", ?" : "?"; params.push_back(std::to_string(ids[i])); }
    sql += ")";
    const auto r = db_.execute(sql, params);
    return r.ok ? static_cast<int>(r.affected_rows) : 0;
}

int MySqlNotificationRepository::markAllRead(int userId) {
    const auto r = db_.execute("UPDATE notifications SET is_read = 1 WHERE user_id = ? AND is_read = 0", {std::to_string(userId)});
    return r.ok ? static_cast<int>(r.affected_rows) : 0;
}

} // namespace database
} // namespace uniconnect
