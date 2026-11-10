#include "services/CourseService.h"

namespace uniconnect {
namespace services {

CourseService::CourseService(database::CourseRepository& course_repo,
                             database::EnrollmentRepository& enroll_repo)
    : course_repo_(course_repo), enroll_repo_(enroll_repo) {}

utils::JsonValue CourseService::getHierarchyTree() {
    auto root = utils::JsonValue::object();
    auto univ_arr = utils::JsonValue::array();

    auto universities = course_repo_.getUniversities();
    for (const auto& u : universities) {
        auto u_obj = u.toJson();
        auto coll_arr = utils::JsonValue::array();

        auto colleges = course_repo_.getCollegesByUniversity(u.id);
        for (const auto& c : colleges) {
            auto c_obj = c.toJson();
            auto dept_arr = utils::JsonValue::array();

            auto departments = course_repo_.getDepartmentsByCollege(c.id);
            for (const auto& d : departments) {
                auto d_obj = d.toJson();
                auto course_arr = utils::JsonValue::array();

                auto courses = course_repo_.findCoursesByDepartment(d.id);
                for (const auto& crs : courses) {
                    auto crs_obj = crs.toJson();
                    auto sec_arr = utils::JsonValue::array();

                    auto sections = course_repo_.findSectionsByCourse(crs.id);
                    for (const auto& s : sections) {
                        sec_arr.push_back(s.toJson());
                    }
                    crs_obj["sections"] = sec_arr;
                    course_arr.push_back(crs_obj);
                }
                d_obj["courses"] = course_arr;
                dept_arr.push_back(d_obj);
            }
            c_obj["departments"] = dept_arr;
            coll_arr.push_back(c_obj);
        }
        u_obj["colleges"] = coll_arr;
        univ_arr.push_back(u_obj);
    }

    root["universities"] = univ_arr;
    return root;
}

std::vector<models::Course> CourseService::getAllCourses() {
    return course_repo_.findAllCourses();
}

std::vector<models::Course> CourseService::getCoursesByDepartment(const std::string& dept_id) {
    return course_repo_.findCoursesByDepartment(dept_id);
}

std::optional<models::Course> CourseService::getCourse(const std::string& course_id) {
    return course_repo_.findCourseById(course_id);
}

std::vector<models::Section> CourseService::getSectionsByCourse(const std::string& course_id) {
    return course_repo_.findSectionsByCourse(course_id);
}

std::optional<models::Section> CourseService::getSection(const std::string& section_id) {
    return course_repo_.findSectionById(section_id);
}

bool CourseService::enrollStudent(const std::string& student_id, const std::string& section_id, models::Enrollment& out_enr) {
    return enroll_repo_.enroll(student_id, section_id, out_enr);
}

bool CourseService::dropStudent(const std::string& student_id, const std::string& section_id) {
    return enroll_repo_.drop(student_id, section_id);
}

std::vector<models::Enrollment> CourseService::getStudentEnrollments(const std::string& student_id) {
    return enroll_repo_.findByStudent(student_id);
}

} // namespace services
} // namespace uniconnect
