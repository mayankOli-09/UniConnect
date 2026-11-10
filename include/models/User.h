#pragma once

#include <string>
#include "utils/Json.h"

namespace uniconnect {
namespace models {

enum class UserRole {
    STUDENT,
    INSTRUCTOR,
    TA,
    ADMIN
};

inline std::string userRoleToString(UserRole role) {
    switch (role) {
        case UserRole::STUDENT: return "STUDENT";
        case UserRole::INSTRUCTOR: return "INSTRUCTOR";
        case UserRole::TA: return "TA";
        case UserRole::ADMIN: return "ADMIN";
        default: return "STUDENT";
    }
}

inline UserRole stringToUserRole(const std::string& str) {
    if (str == "INSTRUCTOR") return UserRole::INSTRUCTOR;
    if (str == "TA") return UserRole::TA;
    if (str == "ADMIN") return UserRole::ADMIN;
    return UserRole::STUDENT;
}

struct User {
    std::string id;
    std::string email;
    std::string password_hash;
    std::string full_name;
    UserRole role{UserRole::STUDENT};
    std::string department_id;
    std::string created_at;

    utils::JsonValue toJson(bool include_password = false) const {
        auto obj = utils::JsonValue::object();
        obj["id"] = id;
        obj["email"] = email;
        obj["full_name"] = full_name;
        obj["role"] = userRoleToString(role);
        obj["department_id"] = department_id;
        obj["created_at"] = created_at;
        if (include_password) {
            obj["password_hash"] = password_hash;
        }
        return obj;
    }

    static User fromJson(const utils::JsonValue& obj) {
        User u;
        u.id = obj["id"].asString();
        u.email = obj["email"].asString();
        u.password_hash = obj["password_hash"].asString();
        u.full_name = obj["full_name"].asString();
        u.role = stringToUserRole(obj["role"].asString());
        u.department_id = obj["department_id"].asString();
        u.created_at = obj["created_at"].asString();
        return u;
    }
};

} // namespace models
} // namespace uniconnect
