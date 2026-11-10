#include "database/CourseRepository.h"

namespace uniconnect {
namespace database {

CourseRepository::CourseRepository(Database& db) : db_(db) {}

std::vector<models::University> CourseRepository::getUniversities() {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::University> list;
    for (const auto& [_, u] : db_.universities) list.push_back(u);
    return list;
}

std::vector<models::College> CourseRepository::getCollegesByUniversity(const std::string& univ_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::College> list;
    for (const auto& [_, c] : db_.colleges) {
        if (c.university_id == univ_id) list.push_back(c);
    }
    return list;
}

std::vector<models::Department> CourseRepository::getDepartmentsByCollege(const std::string& college_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::Department> list;
    for (const auto& [_, d] : db_.departments) {
        if (d.college_id == college_id) list.push_back(d);
    }
    return list;
}

std::optional<models::Course> CourseRepository::findCourseById(const std::string& id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    auto it = db_.courses.find(id);
    if (it != db_.courses.end()) return it->second;
    return std::nullopt;
}

std::vector<models::Course> CourseRepository::findCoursesByDepartment(const std::string& dept_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::Course> list;
    auto it = db_.courses_by_dept.find(dept_id);
    if (it != db_.courses_by_dept.end()) {
        for (const auto& cid : it->second) {
            auto cit = db_.courses.find(cid);
            if (cit != db_.courses.end()) list.push_back(cit->second);
        }
    }
    return list;
}

std::vector<models::Course> CourseRepository::findAllCourses() {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::Course> list;
    for (const auto& [_, c] : db_.courses) list.push_back(c);
    return list;
}

bool CourseRepository::saveCourse(const models::Course& course) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    db_.courses[course.id] = course;
    db_.courses_by_dept[course.department_id].push_back(course.id);
    return true;
}

std::optional<models::Section> CourseRepository::findSectionById(const std::string& id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    auto it = db_.sections.find(id);
    if (it != db_.sections.end()) return it->second;
    return std::nullopt;
}

std::vector<models::Section> CourseRepository::findSectionsByCourse(const std::string& course_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::Section> list;
    auto it = db_.sections_by_course.find(course_id);
    if (it != db_.sections_by_course.end()) {
        for (const auto& sid : it->second) {
            auto sit = db_.sections.find(sid);
            if (sit != db_.sections.end()) list.push_back(sit->second);
        }
    }
    return list;
}

std::vector<models::Section> CourseRepository::findAllSections() {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::Section> list;
    for (const auto& [_, s] : db_.sections) list.push_back(s);
    return list;
}

bool CourseRepository::saveSection(const models::Section& section) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    db_.sections[section.id] = section;
    db_.sections_by_course[section.course_id].push_back(section.id);
    return true;
}

} // namespace database
} // namespace uniconnect
