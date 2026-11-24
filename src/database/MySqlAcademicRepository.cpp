#include "database/MySqlAcademicRepository.h"

#include <stdexcept>

#include "utils/Logger.h"

namespace uniconnect {
namespace database {

namespace {

EnrollResult fail(int status, const std::string& msg) {
    EnrollResult r;
    r.status = status;
    r.error = msg;
    return r;
}
std::string str(int v) { return std::to_string(v); }

struct Conversation { int id = 0; std::string name; };

// Finds the group for (type, course, section, department), or creates it and seeds its faculty/admin members.
// Groups are therefore generated from the hierarchy the first time anyone needs them.
std::optional<Conversation> ensureConversation(MySqlDatabase::Transaction& tx, const std::string& type, const DbValue& courseId,
                                               const DbValue& sectionId, const DbValue& deptId, const std::string& newName, int creatorUserId) {
    auto found = tx.execute(
        "SELECT conversation_id, conversation_name FROM conversations "
        "WHERE conversation_type = ? AND course_id <=> ? AND section_id <=> ? AND department_id <=> ? "
        "ORDER BY conversation_id LIMIT 1",
        {type, courseId, sectionId, deptId});
    if (!found.ok) return std::nullopt;
    if (!found.rows.empty()) return Conversation{rowInt(found.rows[0], "conversation_id"), rowStr(found.rows[0], "conversation_name")};

    auto ins = tx.execute(
        "INSERT INTO conversations (conversation_name, conversation_type, course_id, section_id, department_id, created_by) VALUES (?, ?, ?, ?, ?, ?)",
        {newName, type, courseId, sectionId, deptId, str(creatorUserId)});
    if (!ins.ok) return std::nullopt;
    const std::string id = std::to_string(ins.insert_id);

    DbResult seed;
    if (type == "COURSE") {
        seed = tx.execute("INSERT IGNORE INTO conversation_members (conversation_id, user_id) "
                          "SELECT ?, f.user_id FROM faculty_assignments fa JOIN faculty f ON f.faculty_id = fa.faculty_id "
                          "WHERE fa.course_id = ? AND fa.section_id = ?", {id, courseId, sectionId});
    } else if (type == "SECTION") {
        seed = tx.execute("INSERT IGNORE INTO conversation_members (conversation_id, user_id) "
                          "SELECT DISTINCT ?, f.user_id FROM faculty_assignments fa JOIN faculty f ON f.faculty_id = fa.faculty_id "
                          "WHERE fa.section_id = ?", {id, sectionId});
    } else if (type == "DEPARTMENT") {
        seed = tx.execute("INSERT IGNORE INTO conversation_members (conversation_id, user_id) "
                          "SELECT ?, user_id FROM faculty WHERE department_id = ?", {id, deptId});
        if (seed.ok) seed = tx.execute("INSERT IGNORE INTO conversation_members (conversation_id, user_id) "
                                       "SELECT ?, user_id FROM users WHERE role = 'ADMIN'", {id});
    } else {
        seed.ok = true;
    }
    if (!seed.ok) return std::nullopt;
    return Conversation{static_cast<int>(ins.insert_id), newName};
}

bool addMember(MySqlDatabase::Transaction& tx, int conversationId, int userId) {
    return tx.execute("INSERT IGNORE INTO conversation_members (conversation_id, user_id) VALUES (?, ?)", {str(conversationId), str(userId)}).ok;
}

} // namespace

// ------------------------------------------------------------------ reading
AcademicData MySqlAcademicRepository::loadAcademicData() {
    AcademicData d;
    auto deps = db_.execute("SELECT department_id, department_name FROM departments ORDER BY department_id");
    auto progs = db_.execute("SELECT program_id, program_name, department_id FROM programs ORDER BY program_id");
    auto secs = db_.execute("SELECT section_id, section_name, program_id, `year`, academic_year FROM sections ORDER BY program_id, `year`, section_name");
    auto courses = db_.execute("SELECT c.course_id, c.course_code, c.course_name, c.credits, c.department_id, d.department_name "
                               "FROM courses c JOIN departments d ON d.department_id = c.department_id ORDER BY c.course_code");
    if (!deps.ok || !progs.ok || !secs.ok || !courses.ok) throw std::runtime_error("Could not read the academic data");
    for (const auto& r : deps.rows) d.departments.push_back({rowInt(r, "department_id"), rowStr(r, "department_name")});
    for (const auto& r : progs.rows) d.programs.push_back({rowInt(r, "program_id"), rowStr(r, "program_name"), rowInt(r, "department_id")});
    for (const auto& r : secs.rows) d.sections.push_back({rowInt(r, "section_id"), rowStr(r, "section_name"), rowInt(r, "program_id"), rowInt(r, "year"), rowStr(r, "academic_year")});
    for (const auto& r : courses.rows)
        d.courses.push_back({rowInt(r, "course_id"), rowStr(r, "course_code"), rowStr(r, "course_name"), rowInt(r, "credits"), rowInt(r, "department_id"), rowStr(r, "department_name")});
    return d;
}

// ------------------------------------------------------------------ enroll
EnrollResult MySqlAcademicRepository::enroll(int studentId, int sectionId, std::optional<int> courseId) {
    MySqlDatabase::Transaction tx(db_);
    if (!tx.ok()) return fail(503, "The database is not available. Please try again later.");

    const auto student = tx.execute("SELECT user_id FROM students WHERE student_id = ?", {str(studentId)});
    if (!student.ok) return fail(500, "Could not read the student record.");
    if (student.rows.empty()) return fail(404, "Student not found.");
    const int userId = rowInt(student.rows[0], "user_id");

    const auto sec = tx.execute("SELECT sec.section_name, p.department_id, p.program_name FROM sections sec "
                                "JOIN programs p ON p.program_id = sec.program_id WHERE sec.section_id = ?", {str(sectionId)});
    if (!sec.ok) return fail(500, "Could not read the section.");
    if (sec.rows.empty()) return fail(404, "Section not found.");
    const std::string sectionName = rowStr(sec.rows[0], "section_name");
    const int deptId = rowInt(sec.rows[0], "department_id");
    const std::string programName = rowStr(sec.rows[0], "program_name");

    // A section offers the courses of its department.
    DbRow course;
    if (courseId) {
        const auto c = tx.execute("SELECT course_id, course_code, course_name, department_id FROM courses WHERE course_id = ?", {str(*courseId)});
        if (!c.ok) return fail(500, "Could not read the course.");
        if (c.rows.empty()) return fail(404, "Course not found.");
        course = c.rows[0];
        if (rowInt(course, "department_id") != deptId)
            return fail(400, rowStr(course, "course_code") + " is not offered in Section " + sectionName + " (" + programName + ").");
    } else {
        const auto cs = tx.execute("SELECT course_id, course_code, course_name, department_id FROM courses WHERE department_id = ? ORDER BY course_code", {str(deptId)});
        if (!cs.ok) return fail(500, "Could not read the courses.");
        if (cs.rows.empty()) return fail(400, "This section has no courses yet.");
        if (cs.rows.size() > 1) {
            std::string list;
            for (const auto& r : cs.rows) list += (list.empty() ? "" : ", ") + rowStr(r, "course_code");
            return fail(400, "This section offers several courses. Choose one: " + list);
        }
        course = cs.rows[0];
    }
    const int cid = rowInt(course, "course_id");
    const std::string label = rowStr(course, "course_code") + " (" + rowStr(course, "course_name") + ")";

    const auto existing = tx.execute("SELECT enrollment_id, status FROM enrollments WHERE student_id = ? AND course_id = ? FOR UPDATE", {str(studentId), str(cid)});
    if (!existing.ok) return fail(500, "Could not read the enrollment.");
    if (!existing.rows.empty()) {
        if (rowStr(existing.rows[0], "status") == "ACTIVE") return fail(409, "You are already enrolled in " + label + ".");
        const auto up = tx.execute("UPDATE enrollments SET status = 'ACTIVE', section_id = ?, enrollment_date = CURDATE() WHERE enrollment_id = ?",
                                   {str(sectionId), str(rowInt(existing.rows[0], "enrollment_id"))});
        if (!up.ok) return fail(500, "Could not update the enrollment.");
    } else {
        const auto ins = tx.execute("INSERT INTO enrollments (student_id, course_id, section_id, enrollment_date, status) VALUES (?, ?, ?, CURDATE(), 'ACTIVE')",
                                    {str(studentId), str(cid), str(sectionId)});
        if (!ins.ok) return fail(ins.error_code == 1062 ? 409 : 500, ins.error_code == 1062 ? "You are already enrolled in " + label + "." : "Could not save the enrollment.");
    }

    // Groups follow the enrollment.
    int creator = userId;
    const auto admin = tx.execute("SELECT user_id FROM users WHERE role = 'ADMIN' ORDER BY user_id LIMIT 1");
    if (admin.ok && !admin.rows.empty()) creator = rowInt(admin.rows[0], "user_id");
    const auto dept = tx.execute("SELECT department_name FROM departments WHERE department_id = ?", {str(deptId)});
    const std::string deptName = dept.ok && !dept.rows.empty() ? rowStr(dept.rows[0], "department_name") : "Department";

    const DbValue cidV = str(cid), secV = str(sectionId), deptV = str(deptId), none = std::nullopt;
    const auto courseGroup = ensureConversation(tx, "COURSE", cidV, secV, none, rowStr(course, "course_name") + " - Section " + sectionName, creator);
    const auto sectionGroup = ensureConversation(tx, "SECTION", none, secV, none, programName + " Section " + sectionName, creator);
    const auto deptGroup = ensureConversation(tx, "DEPARTMENT", none, none, deptV, deptName + " Department", creator);
    if (!courseGroup || !sectionGroup || !deptGroup) return fail(500, "Could not create the groups for this section.");

    EnrollResult r;
    for (const auto* g : {&*courseGroup, &*sectionGroup, &*deptGroup}) {
        if (!addMember(tx, g->id, userId)) return fail(500, "Could not add you to the groups.");
        r.groups.push_back(g->name);
    }
    if (!tx.commit()) return fail(500, "Could not save the enrollment.");
    r.ok = true;
    r.message = "Enrolled in " + label + ", Section " + sectionName + ". You were added to " + std::to_string(r.groups.size()) + " groups.";
    return r;
}

// ------------------------------------------------------------------ drop
EnrollResult MySqlAcademicRepository::drop(int studentId, int sectionId, std::optional<int> courseId) {
    MySqlDatabase::Transaction tx(db_);
    if (!tx.ok()) return fail(503, "The database is not available. Please try again later.");

    const auto student = tx.execute("SELECT user_id, section_id FROM students WHERE student_id = ?", {str(studentId)});
    if (!student.ok) return fail(500, "Could not read the student record.");
    if (student.rows.empty()) return fail(404, "Student not found.");
    const int userId = rowInt(student.rows[0], "user_id");
    const int homeSection = rowInt(student.rows[0], "section_id");

    std::string sql = "SELECT e.enrollment_id, e.course_id, c.course_code, c.course_name, sec.section_name FROM enrollments e "
                      "JOIN courses c ON c.course_id = e.course_id JOIN sections sec ON sec.section_id = e.section_id "
                      "WHERE e.student_id = ? AND e.section_id = ? AND e.status = 'ACTIVE'";
    std::vector<DbValue> params = {str(studentId), str(sectionId)};
    if (courseId) { sql += " AND e.course_id = ?"; params.push_back(str(*courseId)); }
    sql += " FOR UPDATE";
    const auto active = tx.execute(sql, params);
    if (!active.ok) return fail(500, "Could not read the enrollment.");
    if (active.rows.empty()) return fail(404, "No active enrollment found for that section.");
    if (active.rows.size() > 1) {
        std::string list;
        for (const auto& r : active.rows) list += (list.empty() ? "" : ", ") + rowStr(r, "course_code");
        return fail(400, "You are enrolled in several courses in this section. Choose one to drop: " + list);
    }
    const DbRow& row = active.rows[0];
    const int cid = rowInt(row, "course_id");
    const std::string label = rowStr(row, "course_code") + " (" + rowStr(row, "course_name") + ")";

    if (!tx.execute("UPDATE enrollments SET status = 'DROPPED' WHERE enrollment_id = ?", {str(rowInt(row, "enrollment_id"))}).ok)
        return fail(500, "Could not update the enrollment.");

    EnrollResult r;
    // Leave the course group.
    const auto cg = tx.execute("SELECT cm.conversation_id, c.conversation_name FROM conversation_members cm JOIN conversations c ON c.conversation_id = cm.conversation_id "
                               "WHERE cm.user_id = ? AND c.conversation_type = 'COURSE' AND c.course_id = ? AND c.section_id = ?", {str(userId), str(cid), str(sectionId)});
    if (!cg.ok) return fail(500, "Could not update your groups.");
    for (const auto& g : cg.rows) {
        if (!tx.execute("DELETE FROM conversation_members WHERE conversation_id = ? AND user_id = ?", {str(rowInt(g, "conversation_id")), str(userId)}).ok)
            return fail(500, "Could not update your groups.");
        r.groups.push_back(rowStr(g, "conversation_name"));
    }
    // Leave the section group too, unless it is still the student's section (home section or another active course there).
    const auto others = tx.execute("SELECT COUNT(*) AS n FROM enrollments WHERE student_id = ? AND section_id = ? AND status = 'ACTIVE'", {str(studentId), str(sectionId)});
    if (!others.ok) return fail(500, "Could not update your groups.");
    if (rowInt(others.rows[0], "n") == 0 && homeSection != sectionId) {
        const auto sg = tx.execute("SELECT cm.conversation_id, c.conversation_name FROM conversation_members cm JOIN conversations c ON c.conversation_id = cm.conversation_id "
                                   "WHERE cm.user_id = ? AND c.conversation_type = 'SECTION' AND c.section_id = ? AND c.course_id IS NULL", {str(userId), str(sectionId)});
        if (!sg.ok) return fail(500, "Could not update your groups.");
        for (const auto& g : sg.rows) {
            if (!tx.execute("DELETE FROM conversation_members WHERE conversation_id = ? AND user_id = ?", {str(rowInt(g, "conversation_id")), str(userId)}).ok)
                return fail(500, "Could not update your groups.");
            r.groups.push_back(rowStr(g, "conversation_name"));
        }
    }
    if (!tx.commit()) return fail(500, "Could not save the change.");
    r.ok = true;
    r.message = "Dropped " + label + ", Section " + rowStr(row, "section_name") + ". You left " + std::to_string(r.groups.size()) + " group(s).";
    return r;
}

} // namespace database
} // namespace uniconnect
