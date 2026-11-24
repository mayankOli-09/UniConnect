#pragma once
#include <string>
#include <vector>

#include "utils/Json.h"

namespace uniconnect {
namespace models {

// A row of `conversations` (course, section, department, user-made and private chats), as one person sees it.
struct ChatGroup {
    int id = 0;
    std::string name;            // for a PRIVATE chat: the other person's name
    std::string type;            // PRIVATE, GROUP, COURSE, SECTION or DEPARTMENT
    int course_id = 0, section_id = 0, department_id = 0;   // 0 = not set
    int member_count = 0;
    bool muted = false;          // this person muted the chat
    int unread_count = 0;        // messages from others after this person's read position
    int other_user_id = 0;       // PRIVATE chats: the person on the other side
    std::string last_text, last_sender, last_at;             // preview of the newest message

    utils::JsonValue toJson() const {
        auto o = utils::JsonValue::object();
        o["group_id"] = id;
        o["name"] = name;
        o["type"] = type;
        if (course_id > 0) o["course_id"] = course_id;
        if (section_id > 0) o["section_id"] = section_id;
        if (department_id > 0) o["department_id"] = department_id;
        o["member_count"] = member_count;
        o["muted"] = muted;
        o["unread_count"] = unread_count;
        if (other_user_id > 0) o["other_user_id"] = other_user_id;
        if (!last_at.empty()) {
            auto m = utils::JsonValue::object();
            m["content"] = last_text;
            m["sender_name"] = last_sender;
            m["sent_at"] = last_at;
            o["last_message"] = m;
        }
        return o;
    }
};

// Someone you can start a chat with.
struct DirectoryUser {
    int id = 0;
    std::string name;
    std::string role;
};

// A row of `messages`, with the sender's name filled in.
struct ChatMessage {
    int id = 0;
    int group_id = 0;
    int sender_id = 0;
    std::string sender_name;
    std::string content;
    std::string sent_at;         // ISO 8601, UTC
    bool is_announcement = false;        // posted with #Announcement by faculty/admin
    bool is_all = false;                 // posted with @all by faculty/admin
    std::vector<int> mentioned_ids;      // people tagged with @Name

    utils::JsonValue toJson() const {
        auto o = utils::JsonValue::object();
        o["message_id"] = id;
        o["group_id"] = group_id;
        o["sender_id"] = sender_id;
        o["sender_name"] = sender_name;
        o["content"] = content;
        o["sent_at"] = sent_at;
        if (is_announcement) o["is_announcement"] = true;
        if (is_all) o["is_all"] = true;
        if (!mentioned_ids.empty()) {
            auto a = utils::JsonValue::array();
            for (int id : mentioned_ids) a.push_back(id);
            o["mentioned_ids"] = a;
        }
        return o;
    }
};

} // namespace models
} // namespace uniconnect
