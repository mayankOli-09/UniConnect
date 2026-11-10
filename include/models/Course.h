#pragma once

#include <string>
#include <vector>
#include "utils/Json.h"

namespace uniconnect {
namespace models {

struct University {
    std::string id;
    std::string name;
    std::string code;

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["id"] = id;
        obj["name"] = name;
        obj["code"] = code;
        return obj;
    }
};

struct College {
    std::string id;
    std::string university_id;
    std::string name;
    std::string code;

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["id"] = id;
        obj["university_id"] = university_id;
        obj["name"] = name;
        obj["code"] = code;
        return obj;
    }
};

struct Department {
    std::string id;
    std::string college_id;
    std::string name;
    std::string code;

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["id"] = id;
        obj["college_id"] = college_id;
        obj["name"] = name;
        obj["code"] = code;
        return obj;
    }
};

struct Course {
    std::string id;
    std::string department_id;
    std::string code;
    std::string title;
    int credits{3};
    int semester{1};

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["id"] = id;
        obj["department_id"] = department_id;
        obj["code"] = code;
        obj["title"] = title;
        obj["credits"] = credits;
        obj["semester"] = semester;
        return obj;
    }

    static Course fromJson(const utils::JsonValue& obj) {
        Course c;
        c.id = obj["id"].asString();
        c.department_id = obj["department_id"].asString();
        c.code = obj["code"].asString();
        c.title = obj["title"].asString();
        c.credits = obj["credits"].asInt(3);
        c.semester = obj["semester"].asInt(1);
        return c;
    }
};

} // namespace models
} // namespace uniconnect
