#pragma once

#include <string>
#include "utils/Json.h"

namespace uniconnect {
namespace models {

struct Notification {
    std::string id;
    std::string user_id;
    std::string title;
    std::string message;
    bool is_read{false};
    int priority{1};
    std::string timestamp;

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["id"] = id;
        obj["user_id"] = user_id;
        obj["title"] = title;
        obj["message"] = message;
        obj["is_read"] = is_read;
        obj["priority"] = priority;
        obj["timestamp"] = timestamp;
        return obj;
    }

    static Notification fromJson(const utils::JsonValue& obj) {
        Notification n;
        n.id = obj["id"].asString();
        n.user_id = obj["user_id"].asString();
        n.title = obj["title"].asString();
        n.message = obj["message"].asString();
        n.is_read = obj["is_read"].asBool(false);
        n.priority = obj["priority"].asInt(1);
        n.timestamp = obj["timestamp"].asString();
        return n;
    }
};

} // namespace models
} // namespace uniconnect
