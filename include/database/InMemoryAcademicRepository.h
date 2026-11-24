#pragma once
// Simplified, non-persistent AcademicRepository for the HTTP tests. The real rules live in MySqlAcademicRepository.
#include <map>
#include <mutex>
#include <set>

#include "database/AcademicRepository.h"

namespace uniconnect {
namespace database {

class InMemoryAcademicRepository : public AcademicRepository {
public:
    AcademicData loadAcademicData() override {
        AcademicData d;
        d.departments = {{1, "Computer Science and Engineering"}, {2, "Mechanical Engineering"}};
        d.programs = {{1, "B.Tech CSE", 1}, {2, "B.Tech ME", 2}};
        d.sections = {{1, "A", 1, 3, "2026-27"}, {2, "B", 1, 3, "2026-27"}, {3, "A", 2, 3, "2026-27"}};
        d.courses = {{1, "CS301", "Database Management Systems", 4, 1, "Computer Science and Engineering"},
                     {2, "CS302", "Operating Systems", 4, 1, "Computer Science and Engineering"}};
        return d;
    }
    EnrollResult enroll(int student, int section, std::optional<int> course) override {
        std::lock_guard<std::mutex> l(m_);
        if (student > 1000) return fail(404, "Student not found.");
        if (section < 1 || section > 3) return fail(404, "Section not found.");
        if (section == 3) return fail(400, "This section has no courses yet.");
        if (!course) return fail(400, "This section offers several courses. Choose one: CS301, CS302");
        if (*course < 1 || *course > 2) return fail(404, "Course not found.");
        if (!active_.insert({student, *course}).second) return fail(409, "Already enrolled in this course.");
        EnrollResult r; r.ok = true; r.message = "Enrolled."; r.groups = {"Course group", "Section group", "Department group"};
        return r;
    }
    EnrollResult drop(int student, int section, std::optional<int> course) override {
        std::lock_guard<std::mutex> l(m_);
        (void)section;
        if (!course || !active_.erase({student, *course})) return fail(404, "No active enrollment found for that section.");
        EnrollResult r; r.ok = true; r.message = "Dropped."; r.groups = {"Course group"};
        return r;
    }

private:
    static EnrollResult fail(int status, const std::string& msg) { EnrollResult r; r.status = status; r.error = msg; return r; }
    std::mutex m_;
    std::set<std::pair<int, int>> active_;
};

} // namespace database
} // namespace uniconnect
