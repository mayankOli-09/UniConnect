#pragma once

#include <string>
#include "utils/Json.h"

namespace uniconnect {
namespace models {

enum class EnrollmentStatus {
    ENROLLED,
    DROPPED,
    COMPLETED
};

inline std::string enrollmentStatusToString(EnrollmentStatus status) {
    switch (status) {
        case EnrollmentStatus::ENROLLED: return "ENROLLED";
        case EnrollmentStatus::DROPPED: return "DROPPED";
        case EnrollmentStatus::COMPLETED: return "COMPLETED";
        default: return "ENROLLED";
    }
}

inline EnrollmentStatus stringToEnrollmentStatus(const std::string& str) {
    if (str == "DROPPED") return EnrollmentStatus::DROPPED;
    if (str == "COMPLETED") return EnrollmentStatus::COMPLETED;
    return EnrollmentStatus::ENROLLED;
}

struct Section {
    std::string id;
    std::string course_id;
    std::string section_name;
    std::string instructor_id;
    int max_capacity{60};
    int current_enrolled{0};
    int semester{1};
    int year{2026};

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["id"] = id;
        obj["course_id"] = course_id;
        obj["section_name"] = section_name;
        obj["instructor_id"] = instructor_id;
        obj["max_capacity"] = max_capacity;
        obj["current_enrolled"] = current_enrolled;
        obj["semester"] = semester;
        obj["year"] = year;
        return obj;
    }

    static Section fromJson(const utils::JsonValue& obj) {
        Section s;
        s.id = obj["id"].asString();
        s.course_id = obj["course_id"].asString();
        s.section_name = obj["section_name"].asString();
        s.instructor_id = obj["instructor_id"].asString();
        s.max_capacity = obj["max_capacity"].asInt(60);
        s.current_enrolled = obj["current_enrolled"].asInt(0);
        s.semester = obj["semester"].asInt(1);
        s.year = obj["year"].asInt(2026);
        return s;
    }
};

struct Enrollment {
    std::string id;
    std::string student_id;
    std::string section_id;
    EnrollmentStatus status{EnrollmentStatus::ENROLLED};
    std::string enrolled_at;

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["id"] = id;
        obj["student_id"] = student_id;
        obj["section_id"] = section_id;
        obj["status"] = enrollmentStatusToString(status);
        obj["enrolled_at"] = enrolled_at;
        return obj;
    }

    static Enrollment fromJson(const utils::JsonValue& obj) {
        Enrollment e;
        e.id = obj["id"].asString();
        e.student_id = obj["student_id"].asString();
        e.section_id = obj["section_id"].asString();
        e.status = stringToEnrollmentStatus(obj["status"].asString());
        e.enrolled_at = obj["enrolled_at"].asString();
        return e;
    }
};

} // namespace models
} // namespace uniconnect
