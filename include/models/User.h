#pragma once
#include <cctype>
#include <string>

#include "utils/Json.h"

namespace uniconnect {
namespace models {

enum class UserRole { STUDENT, FACULTY, ADMIN };   // matches users_chk_1

inline std::string userRoleToString(UserRole r) {
    switch (r) {
        case UserRole::FACULTY: return "FACULTY";
        case UserRole::ADMIN:   return "ADMIN";
        default:                return "STUDENT";
    }
}

// Unknown text maps to STUDENT, the least-privileged role.
inline UserRole stringToUserRole(const std::string& s) {
    std::string u;
    for (char c : s) u += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (u == "FACULTY") return UserRole::FACULTY;
    if (u == "ADMIN") return UserRole::ADMIN;
    return UserRole::STUDENT;
}

// One row of `users`, plus the profile ids from `students` / `faculty` (0 = not applicable).
struct User {
    int id = 0;                    // users.user_id
    std::string name;
    std::string email;
    std::string password_hash;     // never sent to clients
    UserRole role = UserRole::STUDENT;
    int department_id = 0;         // faculty.department_id, or the student's section -> program -> department
    int student_id = 0;            // students.student_id
    int faculty_id = 0;            // faculty.faculty_id
    int section_id = 0;            // students.section_id

    utils::JsonValue toJson() const {
        auto o = utils::JsonValue::object();
        o["user_id"] = id;
        o["name"] = name;
        o["email"] = email;
        o["role"] = userRoleToString(role);
        if (department_id > 0) o["department_id"] = department_id;
        if (student_id > 0) o["student_id"] = student_id;
        if (faculty_id > 0) o["faculty_id"] = faculty_id;
        if (section_id > 0) o["section_id"] = section_id;
        return o;
    }
};

} // namespace models
} // namespace uniconnect
