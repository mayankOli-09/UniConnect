#pragma once
// Reads the tags in a chat message: #Announcement, @all (or @everyone) and @Name.
// Only faculty and administrators can use #Announcement and @all; for students those words are plain text.
#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "models/User.h"

namespace uniconnect {
namespace services {

struct Tags {
    bool announcement = false;
    bool everyone = false;
    std::vector<int> mentioned;     // people tagged with @Name (never the sender)
    bool any() const { return announcement || everyone || !mentioned.empty(); }
};

namespace tags_detail {

inline std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
inline bool isWord(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// True when `needle` appears in `hay` as a whole tag: not glued to a word before it, not continuing into a word after it.
inline bool hasTag(const std::string& hay, const std::string& needle) {
    for (size_t pos = hay.find(needle); pos != std::string::npos; pos = hay.find(needle, pos + 1)) {
        const bool before = pos == 0 || !isWord(hay[pos - 1]);
        const size_t end = pos + needle.size();
        const bool after = end >= hay.size() || !isWord(hay[end]);
        if (before && after) return true;
    }
    return false;
}

// "Dr. Rajesh Kumar" -> "rajesh kumar"
inline std::string withoutTitle(const std::string& name) {
    std::string n = lower(name);
    for (const char* t : {"dr. ", "prof. ", "dr ", "prof "}) {
        const std::string title = t;
        if (n.rfind(title, 0) == 0) { n.erase(0, title.size()); break; }
    }
    return n;
}
inline std::string firstWord(const std::string& s) { return s.substr(0, s.find(' ')); }

} // namespace tags_detail

// members: everyone in the chat as (user id, full name).
inline Tags parseTags(const std::string& content, models::UserRole senderRole, int senderId, const std::vector<std::pair<int, std::string>>& members) {
    using namespace tags_detail;
    Tags t;
    const std::string text = lower(content);
    const bool privileged = senderRole == models::UserRole::FACULTY || senderRole == models::UserRole::ADMIN;
    if (privileged) {
        t.announcement = hasTag(text, "#announcement");
        t.everyone = hasTag(text, "@all") || hasTag(text, "@everyone");
    }

    // A first name only counts when it is unique in this chat ("@Bala" is ambiguous if there are two Balas).
    std::vector<int> firstNameCount;
    for (const auto& m : members) {
        const std::string first = firstWord(withoutTitle(m.second));
        firstNameCount.push_back(static_cast<int>(std::count_if(members.begin(), members.end(), [&](const auto& o) { return firstWord(withoutTitle(o.second)) == first; })));
    }
    std::set<int> found;
    for (size_t i = 0; i < members.size() && found.size() < 50; ++i) {
        const auto& [id, name] = members[i];
        if (id == senderId) continue;
        const std::string full = lower(name), plain = withoutTitle(name);
        bool hit = hasTag(text, "@" + full) || hasTag(text, "@" + plain);
        if (!hit && firstNameCount[i] == 1) hit = hasTag(text, "@" + firstWord(plain));
        if (hit) found.insert(id);
    }
    t.mentioned.assign(found.begin(), found.end());
    return t;
}

} // namespace services
} // namespace uniconnect
