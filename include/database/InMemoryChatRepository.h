#pragma once
// Non-persistent ChatRepository for the HTTP tests.
#include <algorithm>
#include <map>
#include <mutex>
#include <set>

#include "database/ChatRepository.h"

namespace uniconnect {
namespace database {

class InMemoryChatRepository : public ChatRepository {
public:
    void addGroup(int id, const std::string& name, const std::string& type, std::set<int> members) {
        std::lock_guard<std::mutex> l(m_);
        models::ChatGroup g; g.id = id; g.name = name; g.type = type;
        groups_[id] = g; members_[id] = std::move(members);
        next_group_ = std::max(next_group_, id + 1);
    }
    void setUserName(int id, const std::string& name) { std::lock_guard<std::mutex> l(m_); names_[id] = name; }
    void addUser(int id, const std::string& name, const std::string& role) { std::lock_guard<std::mutex> l(m_); users_[id] = {id, name, role}; names_[id] = name; }
    void removeMember(int gid, int uid) { std::lock_guard<std::mutex> l(m_); members_[gid].erase(uid); }

    std::vector<models::ChatGroup> groupsForUser(int uid) override {
        std::lock_guard<std::mutex> l(m_);
        std::vector<models::ChatGroup> out;
        for (auto& [id, g] : groups_) if (members_[id].count(uid)) out.push_back(view(id, uid));
        std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.last_at > b.last_at; });   // newest activity first
        return out;
    }
    std::optional<models::ChatGroup> groupFor(int gid, int uid) override {
        std::lock_guard<std::mutex> l(m_);
        if (!groups_.count(gid) || !members_[gid].count(uid)) return std::nullopt;
        return view(gid, uid);
    }
    bool isMember(int gid, int uid) override { std::lock_guard<std::mutex> l(m_); return members_.count(gid) && members_[gid].count(uid); }
    std::vector<int> memberIds(int gid) override { std::lock_guard<std::mutex> l(m_); return {members_[gid].begin(), members_[gid].end()}; }
    std::vector<std::pair<int, std::string>> memberNames(int gid) override {
        std::lock_guard<std::mutex> l(m_);
        std::vector<std::pair<int, std::string>> out;
        for (int id : members_[gid]) out.push_back({id, names_.count(id) ? names_[id] : "User " + std::to_string(id)});
        return out;
    }
    std::string groupType(int gid) override { std::lock_guard<std::mutex> l(m_); return groups_.count(gid) ? groups_[gid].type : ""; }

    std::vector<models::ChatMessage> messages(int gid, int limit, std::optional<int> before) override {
        std::lock_guard<std::mutex> l(m_);
        std::vector<models::ChatMessage> out;
        for (auto it = msgs_.rbegin(); it != msgs_.rend() && static_cast<int>(out.size()) < limit; ++it)
            if (it->group_id == gid && (!before || it->id < *before)) out.push_back(*it);
        return out;
    }
    std::optional<models::ChatMessage> addMessage(int gid, int sender, const std::string& content) override {
        std::lock_guard<std::mutex> l(m_);
        models::ChatMessage m;
        m.id = next_++; m.group_id = gid; m.sender_id = sender; m.sender_name = names_.count(sender) ? names_[sender] : "User " + std::to_string(sender);
        m.content = content; m.sent_at = "2026-10-27T10:00:" + std::string(m.id < 10 ? "0" : "") + std::to_string(m.id % 60) + "Z";
        msgs_.push_back(m);
        return m;
    }

    std::vector<models::DirectoryUser> searchUsers(int exclude, const std::string& q, int limit) override {
        std::lock_guard<std::mutex> l(m_);
        std::vector<models::DirectoryUser> out;
        auto lower = [](std::string s) { for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
        for (auto& [id, u] : users_)
            if (id != exclude && lower(u.name).find(lower(q)) != std::string::npos && static_cast<int>(out.size()) < limit) out.push_back(u);
        return out;
    }
    bool usersExist(const std::vector<int>& ids) override {
        std::lock_guard<std::mutex> l(m_);
        return std::all_of(ids.begin(), ids.end(), [&](int id) { return users_.count(id) > 0; });
    }
    std::optional<models::ChatGroup> findOrCreateDirect(int a, int b) override {
        std::lock_guard<std::mutex> l(m_);
        if (a == b || !users_.count(a) || !users_.count(b)) return std::nullopt;
        for (auto& [id, g] : groups_) if (g.type == "PRIVATE" && members_[id] == std::set<int>({a, b})) return view(id, a);
        const int id = next_group_++;
        models::ChatGroup g; g.id = id; g.type = "PRIVATE";
        groups_[id] = g; members_[id] = {a, b};
        return view(id, a);
    }
    std::optional<models::ChatGroup> createGroup(int creator, const std::string& name, const std::vector<int>& ids) override {
        std::lock_guard<std::mutex> l(m_);
        const int id = next_group_++;
        models::ChatGroup g; g.id = id; g.name = name; g.type = "GROUP";
        groups_[id] = g;
        members_[id] = std::set<int>(ids.begin(), ids.end());
        members_[id].insert(creator);
        return view(id, creator);
    }
    bool leaveGroup(int gid, int uid) override {
        std::lock_guard<std::mutex> l(m_);
        const bool had = members_[gid].erase(uid) > 0;
        if (had && members_[gid].empty()) { groups_.erase(gid); members_.erase(gid); }
        return had;
    }
    void setMuted(int gid, int uid, bool muted) override { std::lock_guard<std::mutex> l(m_); settings_[{gid, uid}].first = muted; }
    int markRead(int gid, int uid, std::optional<int> upto) override {
        std::lock_guard<std::mutex> l(m_);
        int target = 0;
        if (upto) target = *upto;
        else for (auto& m : msgs_) if (m.group_id == gid) target = std::max(target, m.id);
        auto& s = settings_[{gid, uid}];
        s.second = std::max(s.second, target);
        return s.second;
    }

private:
    models::ChatGroup view(int gid, int uid) {   // the group as `uid` sees it (caller holds the lock)
        models::ChatGroup g = groups_[gid];
        g.member_count = static_cast<int>(members_[gid].size());
        if (g.type == "PRIVATE") for (int other : members_[gid]) if (other != uid) { g.other_user_id = other; g.name = names_.count(other) ? names_[other] : "User " + std::to_string(other); }
        auto s = settings_.find({gid, uid});
        g.muted = s != settings_.end() && s->second.first;
        const int read = s == settings_.end() ? 0 : s->second.second;
        for (auto& m : msgs_) if (m.group_id == gid) {
            if (m.id > read && m.sender_id != uid) ++g.unread_count;
            g.last_text = m.content; g.last_sender = m.sender_name; g.last_at = m.sent_at;
        }
        return g;
    }

    std::mutex m_;
    std::map<int, models::ChatGroup> groups_;
    std::map<int, std::set<int>> members_;
    std::map<int, std::string> names_;
    std::map<int, models::DirectoryUser> users_;
    std::map<std::pair<int, int>, std::pair<bool, int>> settings_;   // (group, user) -> (muted, last read id)
    std::vector<models::ChatMessage> msgs_;
    int next_ = 1, next_group_ = 100;
};

} // namespace database
} // namespace uniconnect
