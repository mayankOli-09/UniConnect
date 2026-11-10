#pragma once

#include <string>
#include <vector>
#include <optional>
#include "models/Course.h"
#include "models/Section.h"
#include "database/Database.h"

namespace uniconnect {
namespace database {

class CourseRepository {
public:
    explicit CourseRepository(Database& db = Database::getInstance());

    // Hierarchy
    std::vector<models::University> getUniversities();
    std::vector<models::College> getCollegesByUniversity(const std::string& univ_id);
    std::vector<models::Department> getDepartmentsByCollege(const std::string& college_id);

    // Courses
    std::optional<models::Course> findCourseById(const std::string& id);
    std::vector<models::Course> findCoursesByDepartment(const std::string& dept_id);
    std::vector<models::Course> findAllCourses();
    bool saveCourse(const models::Course& course);

    // Sections
    std::optional<models::Section> findSectionById(const std::string& id);
    std::vector<models::Section> findSectionsByCourse(const std::string& course_id);
    std::vector<models::Section> findAllSections();
    bool saveSection(const models::Section& section);

private:
    Database& db_;
};

} // namespace database
} // namespace uniconnect
