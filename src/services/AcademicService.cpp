#include "services/AcademicService.h"

namespace uniconnect {
namespace services {

using utils::JsonValue;

namespace {
bool parseId(const std::string& s, int& out) {
    if (s.empty() || s.size() > 9) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    out = std::atoi(s.c_str());
    return out > 0;
}
JsonValue courseJson(const database::CourseRow& c) {
    auto o = JsonValue::object();
    o["course_id"] = c.id;
    o["course_code"] = c.code;
    o["course_name"] = c.name;
    o["credits"] = c.credits;
    o["department_id"] = c.department_id;
    if (!c.department_name.empty()) o["department_name"] = c.department_name;
    return o;
}
ServiceResult failure(int status, const std::string& msg) {
    ServiceResult r;
    r.status = status;
    r.error = msg;
    return r;
}
} // namespace

JsonValue AcademicService::courses() {
    auto arr = JsonValue::array();
    for (const auto& c : repo_.loadAcademicData().courses) arr.push_back(courseJson(c));
    auto o = JsonValue::object();
    o["courses"] = arr;
    return o;
}

JsonValue AcademicService::hierarchy() { return buildHierarchy(repo_.loadAcademicData()); }

JsonValue AcademicService::buildHierarchy(const database::AcademicData& d) {
    auto root = JsonValue::object();
    root["name"] = "University";
    auto depts = JsonValue::array();
    for (const auto& dep : d.departments) {
        auto dn = JsonValue::object();
        dn["department_id"] = dep.id;
        dn["name"] = dep.name;
        auto programs = JsonValue::array();
        for (const auto& pr : d.programs) {
            if (pr.department_id != dep.id) continue;
            auto pn = JsonValue::object();
            pn["program_id"] = pr.id;
            pn["name"] = pr.name;
            auto sections = JsonValue::array();
            for (const auto& s : d.sections) {
                if (s.program_id != pr.id) continue;
                auto sn = JsonValue::object();
                sn["section_id"] = s.id;
                sn["name"] = "Section " + s.name + ", year " + std::to_string(s.year) + " (" + s.academic_year + ")";
                sn["year"] = s.year;
                sn["academic_year"] = s.academic_year;
                auto cs = JsonValue::array();
                for (const auto& c : d.courses)           // a section offers the courses of its department
                    if (c.department_id == dep.id) cs.push_back(courseJson(c));
                sn["courses"] = cs;
                sections.push_back(sn);
            }
            pn["sections"] = sections;
            programs.push_back(pn);
        }
        dn["programs"] = programs;
        depts.push_back(dn);
    }
    root["departments"] = depts;
    return root;
}

ServiceResult AcademicService::enroll(const models::User& c, const std::string& st, const std::string& sec, const std::string& co) { return change(true, c, st, sec, co); }
ServiceResult AcademicService::drop(const models::User& c, const std::string& st, const std::string& sec, const std::string& co) { return change(false, c, st, sec, co); }

ServiceResult AcademicService::change(bool enroll, const models::User& caller, const std::string& studentText,
                                      const std::string& sectionText, const std::string& courseText) {
    if (caller.role == models::UserRole::FACULTY) return failure(403, "Only students and administrators can change enrollments.");

    int studentId = 0;
    if (caller.role == models::UserRole::STUDENT) {
        if (caller.student_id <= 0) return failure(403, "Your account has no student profile.");
        if (!studentText.empty() && (!parseId(studentText, studentId) || studentId != caller.student_id))
            return failure(403, "You can only change your own enrollment.");
        studentId = caller.student_id;
    } else if (!parseId(studentText, studentId)) {
        return failure(400, "A valid student_id is required.");
    }

    int sectionId = 0;
    if (!parseId(sectionText, sectionId)) return failure(400, "A valid section_id is required.");
    std::optional<int> courseId;
    if (!courseText.empty()) {
        int cid = 0;
        if (!parseId(courseText, cid)) return failure(400, "Invalid course_id.");
        courseId = cid;
    }

    const auto r = enroll ? repo_.enroll(studentId, sectionId, courseId) : repo_.drop(studentId, sectionId, courseId);
    if (!r.ok) return failure(r.status, r.error);

    ServiceResult out;
    auto o = JsonValue::object();
    o["success"] = true;
    o["message"] = r.message;
    auto g = JsonValue::array();
    for (const auto& name : r.groups) g.push_back(name);
    o["groups"] = g;
    out.body = o;
    return out;
}

} // namespace services
} // namespace uniconnect
