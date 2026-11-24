#include "database/MySqlChatRepository.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace uniconnect {
namespace database {

namespace {
// The connection runs in UTC, so this is an ISO-8601 instant the browser can convert to local time.
const char* kMessageColumns =
    "m.message_id, m.conversation_id, m.sender_id, u.name AS sender_name, m.message_text, "
    "DATE_FORMAT(m.sent_at, '%Y-%m-%dT%H:%i:%sZ') AS sent_at ";

models::ChatMessage toMessage(const DbRow& r) {
    models::ChatMessage m;
    m.id = rowInt(r, "message_id");
    m.group_id = rowInt(r, "conversation_id");
    m.sender_id = rowInt(r, "sender_id");
    m.sender_name = rowStr(r, "sender_name");
    m.content = rowStr(r, "message_text");
    m.sent_at = rowStr(r, "sent_at");
    return m;
}

std::string placeholders(size_t n) {
    std::string s;
    for (size_t i = 0; i < n; ++i) s += i ? ", ?" : "?";
    return s;
}
} // namespace

// ------------------------------------------------------------------ groups
// One query gives every group a person is in, with their mute flag, unread count, newest-message preview and
// (for private chats) the other person. Newest activity comes first.
std::vector<models::ChatGroup> MySqlChatRepository::queryGroups(int userId, std::optional<int> groupId) {
    std::string sql =
        "SELECT c.conversation_id, c.conversation_name, c.conversation_type, c.course_id, c.section_id, c.department_id, "
        "       (SELECT COUNT(*) FROM conversation_members x WHERE x.conversation_id = c.conversation_id) AS member_count, "
        "       COALESCE(s.is_muted, 0) AS is_muted, "
        "       (SELECT COUNT(*) FROM messages mu WHERE mu.conversation_id = c.conversation_id "
        "          AND mu.message_id > COALESCE(s.last_read_message_id, 0) AND mu.sender_id <> ?) AS unread_count, "
        "       lm.message_text AS last_text, lu.name AS last_sender, DATE_FORMAT(lm.sent_at, '%Y-%m-%dT%H:%i:%sZ') AS last_at, "
        "       ou.user_id AS other_user_id, ou.name AS other_name "
        "FROM conversation_members m "
        "JOIN conversations c ON c.conversation_id = m.conversation_id "
        "LEFT JOIN conversation_settings s ON s.conversation_id = c.conversation_id AND s.user_id = m.user_id "
        "LEFT JOIN messages lm ON lm.message_id = (SELECT MAX(l2.message_id) FROM messages l2 WHERE l2.conversation_id = c.conversation_id) "
        "LEFT JOIN users lu ON lu.user_id = lm.sender_id "
        "LEFT JOIN users ou ON c.conversation_type = 'PRIVATE' AND ou.user_id = "
        "     (SELECT m2.user_id FROM conversation_members m2 WHERE m2.conversation_id = c.conversation_id AND m2.user_id <> m.user_id LIMIT 1) "
        "WHERE m.user_id = ?";
    std::vector<DbValue> params = {std::to_string(userId), std::to_string(userId)};
    if (groupId) { sql += " AND c.conversation_id = ?"; params.push_back(std::to_string(*groupId)); }
    sql += " ORDER BY (lm.message_id IS NULL), lm.sent_at DESC, lm.message_id DESC, c.conversation_name, c.conversation_id";

    const auto r = db_.execute(sql, params);
    if (!r.ok) throw std::runtime_error("Could not read the groups");
    std::vector<models::ChatGroup> out;
    for (const auto& row : r.rows) {
        models::ChatGroup g;
        g.id = rowInt(row, "conversation_id");
        g.type = rowStr(row, "conversation_type", "GROUP");
        const std::string other = rowStr(row, "other_name");
        g.name = (g.type == "PRIVATE" && !other.empty()) ? other : rowStr(row, "conversation_name", "Group " + std::to_string(g.id));
        g.course_id = rowInt(row, "course_id");
        g.section_id = rowInt(row, "section_id");
        g.department_id = rowInt(row, "department_id");
        g.member_count = rowInt(row, "member_count");
        g.muted = rowInt(row, "is_muted") != 0;
        g.unread_count = rowInt(row, "unread_count");
        g.other_user_id = rowInt(row, "other_user_id");
        g.last_text = rowStr(row, "last_text");
        g.last_sender = rowStr(row, "last_sender");
        g.last_at = rowStr(row, "last_at");
        out.push_back(g);
    }
    return out;
}

std::vector<models::ChatGroup> MySqlChatRepository::groupsForUser(int userId) { return queryGroups(userId, std::nullopt); }

std::optional<models::ChatGroup> MySqlChatRepository::groupFor(int groupId, int userId) {
    auto v = queryGroups(userId, groupId);
    if (v.empty()) return std::nullopt;
    return v.front();
}

bool MySqlChatRepository::isMember(int groupId, int userId) {
    const auto r = db_.execute("SELECT 1 AS present FROM conversation_members WHERE conversation_id = ? AND user_id = ?",
                               {std::to_string(groupId), std::to_string(userId)});
    return r.ok && !r.rows.empty();
}

std::vector<int> MySqlChatRepository::memberIds(int groupId) {
    std::vector<int> ids;
    const auto r = db_.execute("SELECT user_id FROM conversation_members WHERE conversation_id = ?", {std::to_string(groupId)});
    if (r.ok) for (const auto& row : r.rows) ids.push_back(rowInt(row, "user_id"));
    return ids;
}

std::vector<std::pair<int, std::string>> MySqlChatRepository::memberNames(int groupId) {
    std::vector<std::pair<int, std::string>> out;
    const auto r = db_.execute("SELECT u.user_id, u.name FROM conversation_members m JOIN users u ON u.user_id = m.user_id WHERE m.conversation_id = ? ORDER BY u.name, u.user_id",
                               {std::to_string(groupId)});
    if (r.ok) for (const auto& row : r.rows) out.push_back({rowInt(row, "user_id"), rowStr(row, "name")});
    return out;
}

std::string MySqlChatRepository::groupType(int groupId) {
    const auto r = db_.execute("SELECT conversation_type FROM conversations WHERE conversation_id = ?", {std::to_string(groupId)});
    return r.ok && !r.rows.empty() ? rowStr(r.rows[0], "conversation_type") : "";
}

// ------------------------------------------------------------------ messages
std::vector<models::ChatMessage> MySqlChatRepository::messages(int groupId, int limit, std::optional<int> beforeId) {
    std::string sql = std::string("SELECT ") + kMessageColumns +
                      "FROM messages m JOIN users u ON u.user_id = m.sender_id WHERE m.conversation_id = ?";
    std::vector<DbValue> params = {std::to_string(groupId)};
    if (beforeId) { sql += " AND m.message_id < ?"; params.push_back(std::to_string(*beforeId)); }
    sql += " ORDER BY m.message_id DESC LIMIT ?";
    params.push_back(std::to_string(limit));
    const auto r = db_.execute(sql, params);
    if (!r.ok) throw std::runtime_error("Could not read the messages");
    std::vector<models::ChatMessage> out;
    for (const auto& row : r.rows) out.push_back(toMessage(row));
    return out;
}

std::optional<models::ChatMessage> MySqlChatRepository::addMessage(int groupId, int senderId, const std::string& content) {
    const auto ins = db_.execute("INSERT INTO messages (conversation_id, sender_id, message_text) VALUES (?, ?, ?)",
                                 {std::to_string(groupId), std::to_string(senderId), content});
    if (!ins.ok) return std::nullopt;
    const auto r = db_.execute(std::string("SELECT ") + kMessageColumns +
                               "FROM messages m JOIN users u ON u.user_id = m.sender_id WHERE m.message_id = ?",
                               {std::to_string(ins.insert_id)});
    if (!r.ok || r.rows.empty()) return std::nullopt;
    return toMessage(r.rows[0]);
}

// ------------------------------------------------------------------ people and new chats
std::vector<models::DirectoryUser> MySqlChatRepository::searchUsers(int excludeUser, const std::string& query, int limit) {
    std::string like = "%";
    for (char c : query) { if (c == '\\' || c == '%' || c == '_') like += '\\'; like += c; }   // the person's text is never a pattern
    like += "%";
    const auto r = db_.execute("SELECT user_id, name, role FROM users WHERE user_id <> ? AND name LIKE ? ESCAPE '\\\\' ORDER BY name, user_id LIMIT ?",
                               {std::to_string(excludeUser), like, std::to_string(limit)});
    std::vector<models::DirectoryUser> out;
    if (r.ok) for (const auto& row : r.rows) out.push_back({rowInt(row, "user_id"), rowStr(row, "name"), rowStr(row, "role")});
    return out;
}

bool MySqlChatRepository::usersExist(const std::vector<int>& ids) {
    const std::set<int> unique(ids.begin(), ids.end());
    if (unique.empty()) return true;
    std::vector<DbValue> params;
    for (int id : unique) params.push_back(std::to_string(id));
    const auto r = db_.execute("SELECT COUNT(*) AS n FROM users WHERE user_id IN (" + placeholders(unique.size()) + ")", params);
    return r.ok && !r.rows.empty() && rowInt(r.rows[0], "n") == static_cast<int>(unique.size());
}

std::optional<models::ChatGroup> MySqlChatRepository::findOrCreateDirect(int a, int b) {
    if (a == b) return std::nullopt;
    int groupId = 0;
    {
        MySqlDatabase::Transaction tx(db_);
        if (!tx.ok()) return std::nullopt;
        // Lock both people's rows (always lowest id first), so two requests for the same pair cannot both create a chat.
        const auto lock = tx.execute("SELECT user_id FROM users WHERE user_id IN (?, ?) ORDER BY user_id FOR UPDATE",
                                     {std::to_string(std::min(a, b)), std::to_string(std::max(a, b))});
        if (!lock.ok || lock.rows.size() != 2) return std::nullopt;

        const auto found = tx.execute(
            "SELECT c.conversation_id FROM conversations c "
            "JOIN conversation_members x ON x.conversation_id = c.conversation_id AND x.user_id = ? "
            "JOIN conversation_members y ON y.conversation_id = c.conversation_id AND y.user_id = ? "
            "WHERE c.conversation_type = 'PRIVATE' ORDER BY c.conversation_id LIMIT 1", {std::to_string(a), std::to_string(b)});
        if (!found.ok) return std::nullopt;
        if (!found.rows.empty()) {
            groupId = rowInt(found.rows[0], "conversation_id");
        } else {
            const auto ins = tx.execute("INSERT INTO conversations (conversation_name, conversation_type, created_by) VALUES (?, 'PRIVATE', ?)",
                                        {DbValue{}, std::to_string(a)});
            if (!ins.ok) return std::nullopt;
            groupId = static_cast<int>(ins.insert_id);
            for (int who : {a, b})
                if (!tx.execute("INSERT INTO conversation_members (conversation_id, user_id) VALUES (?, ?)", {std::to_string(groupId), std::to_string(who)}).ok)
                    return std::nullopt;
        }
        if (!tx.commit()) return std::nullopt;
    }
    return groupFor(groupId, a);
}

std::optional<models::ChatGroup> MySqlChatRepository::createGroup(int creator, const std::string& name, const std::vector<int>& memberIds) {
    std::set<int> members(memberIds.begin(), memberIds.end());
    members.insert(creator);
    int groupId = 0;
    {
        MySqlDatabase::Transaction tx(db_);
        if (!tx.ok()) return std::nullopt;
        const auto ins = tx.execute("INSERT INTO conversations (conversation_name, conversation_type, created_by) VALUES (?, 'GROUP', ?)",
                                    {name, std::to_string(creator)});
        if (!ins.ok) return std::nullopt;
        groupId = static_cast<int>(ins.insert_id);
        for (int who : members)
            if (!tx.execute("INSERT IGNORE INTO conversation_members (conversation_id, user_id) VALUES (?, ?)", {std::to_string(groupId), std::to_string(who)}).ok)
                return std::nullopt;
        if (!tx.commit()) return std::nullopt;
    }
    return groupFor(groupId, creator);
}

bool MySqlChatRepository::leaveGroup(int groupId, int userId) {
    MySqlDatabase::Transaction tx(db_);
    if (!tx.ok()) return false;
    const auto del = tx.execute("DELETE FROM conversation_members WHERE conversation_id = ? AND user_id = ?", {std::to_string(groupId), std::to_string(userId)});
    if (!del.ok || del.affected_rows == 0) return false;
    const auto left = tx.execute("SELECT COUNT(*) AS n FROM conversation_members WHERE conversation_id = ?", {std::to_string(groupId)});
    if (left.ok && !left.rows.empty() && rowInt(left.rows[0], "n") == 0)            // the last person left: remove the empty group
        tx.execute("DELETE FROM conversations WHERE conversation_id = ?", {std::to_string(groupId)});
    return tx.commit();
}

// ------------------------------------------------------------------ per-person settings
void MySqlChatRepository::setMuted(int groupId, int userId, bool muted) {
    const std::string flag = muted ? "1" : "0";
    db_.execute("INSERT INTO conversation_settings (user_id, conversation_id, is_muted) VALUES (?, ?, ?) ON DUPLICATE KEY UPDATE is_muted = ?",
                {std::to_string(userId), std::to_string(groupId), flag, flag});
}

int MySqlChatRepository::markRead(int groupId, int userId, std::optional<int> upTo) {
    int target = 0;
    if (upTo) {
        target = *upTo;
    } else {
        const auto latest = db_.execute("SELECT COALESCE(MAX(message_id), 0) AS n FROM messages WHERE conversation_id = ?", {std::to_string(groupId)});
        if (latest.ok && !latest.rows.empty()) target = rowInt(latest.rows[0], "n");
    }
    // The read position only ever moves forward.
    db_.execute("INSERT INTO conversation_settings (user_id, conversation_id, last_read_message_id) VALUES (?, ?, ?) "
                "ON DUPLICATE KEY UPDATE last_read_message_id = GREATEST(last_read_message_id, ?)",
                {std::to_string(userId), std::to_string(groupId), std::to_string(target), std::to_string(target)});
    const auto now = db_.execute("SELECT last_read_message_id AS n FROM conversation_settings WHERE user_id = ? AND conversation_id = ?",
                                 {std::to_string(userId), std::to_string(groupId)});
    return now.ok && !now.rows.empty() ? rowInt(now.rows[0], "n") : target;
}

} // namespace database
} // namespace uniconnect
