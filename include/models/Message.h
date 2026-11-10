#pragma once

#include <string>
#include <vector>
#include "utils/Json.h"

namespace uniconnect {
namespace models {

enum class GroupType {
    UNIVERSITY,
    DEPARTMENT,
    COURSE,
    SECTION
};

inline std::string groupTypeToString(GroupType type) {
    switch (type) {
        case GroupType::UNIVERSITY: return "UNIVERSITY";
        case GroupType::DEPARTMENT: return "DEPARTMENT";
        case GroupType::COURSE: return "COURSE";
        case GroupType::SECTION: return "SECTION";
        default: return "SECTION";
    }
}

inline GroupType stringToGroupType(const std::string& str) {
    if (str == "UNIVERSITY") return GroupType::UNIVERSITY;
    if (str == "DEPARTMENT") return GroupType::DEPARTMENT;
    if (str == "COURSE") return GroupType::COURSE;
    return GroupType::SECTION;
}

enum class MessageType {
    TEXT,
    ANNOUNCEMENT,
    ALERT,
    SYSTEM
};

inline std::string messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::TEXT: return "TEXT";
        case MessageType::ANNOUNCEMENT: return "ANNOUNCEMENT";
        case MessageType::ALERT: return "ALERT";
        case MessageType::SYSTEM: return "SYSTEM";
        default: return "TEXT";
    }
}

inline MessageType stringToMessageType(const std::string& str) {
    if (str == "ANNOUNCEMENT") return MessageType::ANNOUNCEMENT;
    if (str == "ALERT") return MessageType::ALERT;
    if (str == "SYSTEM") return MessageType::SYSTEM;
    return MessageType::TEXT;
}

struct CommunicationGroup {
    std::string id;
    GroupType type{GroupType::SECTION};
    std::string target_id; // id of course / section / department
    std::string name;
    std::string description;

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["id"] = id;
        obj["type"] = groupTypeToString(type);
        obj["target_id"] = target_id;
        obj["name"] = name;
        obj["description"] = description;
        return obj;
    }
};

struct Message {
    std::string id;
    std::string group_id;
    std::string sender_id;
    std::string sender_name;
    std::string content;
    MessageType type{MessageType::TEXT};
    int priority{1}; // 1 = normal chat, 5 = announcement, 10 = urgent alert (used by CPU scheduler)
    std::string timestamp;

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["id"] = id;
        obj["group_id"] = group_id;
        obj["sender_id"] = sender_id;
        obj["sender_name"] = sender_name;
        obj["content"] = content;
        obj["type"] = messageTypeToString(type);
        obj["priority"] = priority;
        obj["timestamp"] = timestamp;
        return obj;
    }

    static Message fromJson(const utils::JsonValue& obj) {
        Message m;
        m.id = obj["id"].asString();
        m.group_id = obj["group_id"].asString();
        m.sender_id = obj["sender_id"].asString();
        m.sender_name = obj["sender_name"].asString();
        m.content = obj["content"].asString();
        m.type = stringToMessageType(obj["type"].asString());
        m.priority = obj["priority"].asInt(1);
        m.timestamp = obj["timestamp"].asString();
        return m;
    }
};

} // namespace models
} // namespace uniconnect
