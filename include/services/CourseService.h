#pragma once

#include <string>
#include <vector>
#include <optional>
#include "database/CourseRepository.h"
#include "database/EnrollmentRepository.h"
#include "models/Course.h"
#include "models/Section.h"
#include "utils/Json.h"

namespace uniconnect {
namespace services {

class CourseService {
public:
    CourseService(database::CourseRepository& course_repo,
                  database::EnrollmentRepository& enroll_repo);

    // Full Hierarchy representation
    utils::JsonValue getHierarchyTree();

    std::vector<models::Course> getAllCourses();
    std::vector<models::Course> getCoursesByDepartment(const std::string& dept_id);
    std::optional<models::Course> getCourse(const std::string& course_id);

    std::vector<models::Section> getSectionsByCourse(const std::string& course_id);
    std::optional<models::Section> getSection(const std::string& section_id);

    // Enrollment operations with auto-sync
    bool enrollStudent(const std::string& student_id, const std::string& section_id, models::Enrollment& out_enr);
    bool dropStudent(const std::string& student_id, const std::string& section_id);
    std::vector<models::Enrollment> getStudentEnrollments(const std::string& student_id);

private:
    database::CourseRepository& course_repo_;
    database::EnrollmentRepository& enroll_repo_;
};

} // namespace services
} // namespace uniconnect
