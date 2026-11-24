#pragma once
#include <optional>
#include <string>
#include <vector>

namespace uniconnect {
namespace database {

struct DepartmentRow { int id = 0; std::string name; };
struct ProgramRow { int id = 0; std::string name; int department_id = 0; };
struct SectionRow { int id = 0; std::string name; int program_id = 0; int year = 0; std::string academic_year; };
struct CourseRow { int id = 0; std::string code, name; int credits = 0; int department_id = 0; std::string department_name; };

// Everything the hierarchy view needs, loaded as flat lists.
struct AcademicData {
    std::vector<DepartmentRow> departments;
    std::vector<ProgramRow> programs;
    std::vector<SectionRow> sections;
    std::vector<CourseRow> courses;
};

struct EnrollResult {
    bool ok = false;
    int status = 200;                  // suggested HTTP status (400, 404, 409, 500, 503 when !ok)
    std::string error;
    std::string message;               // shown to the user when ok
    std::vector<std::string> groups;   // groups the student is now in (enroll) or was removed from (drop)
};

// Courses, sections and enrollments. Enrolling and dropping also keep the group memberships in sync, in one transaction.
class AcademicRepository {
public:
    virtual ~AcademicRepository() = default;
    virtual AcademicData loadAcademicData() = 0;
    virtual EnrollResult enroll(int student_id, int section_id, std::optional<int> course_id) = 0;
    virtual EnrollResult drop(int student_id, int section_id, std::optional<int> course_id) = 0;
};

} // namespace database
} // namespace uniconnect
