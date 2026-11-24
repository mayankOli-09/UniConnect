#pragma once
#include <string>

#include "utils/Json.h"

namespace uniconnect {
namespace models {

// A row of `notifications`: one person's copy of an announcement.
struct Notification {
    int id = 0;
    int user_id = 0;
    int announcement_id = 0;     // 0 = not from an announcement
    std::string title;
    std::string content;
    std::string kind = "ANNOUNCEMENT";   // ANNOUNCEMENT (posted in a group), NOTICE (sent by an admin) or MENTION
    bool is_read = false;
    std::string created_at;      // ISO 8601, UTC

    utils::JsonValue toJson() const {
        auto o = utils::JsonValue::object();
        o["notification_id"] = id;
        if (announcement_id > 0) o["announcement_id"] = announcement_id;
        o["title"] = title;
        o["content"] = content;
        o["kind"] = kind;
        o["is_read"] = is_read;
        o["created_at"] = created_at;
        return o;
    }
};

} // namespace models
} // namespace uniconnect
