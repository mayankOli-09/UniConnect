#include "database/Database.h"
#include "utils/Crypto.h"
#include "utils/Logger.h"

namespace uniconnect {
namespace database {

Database& Database::getInstance() {
    static Database instance;
    return instance;
}

Database::Database() {
    seedDefaults();
}

void Database::clear() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    universities.clear();
    colleges.clear();
    departments.clear();
    users.clear();
    courses.clear();
    sections.clear();
    enrollments.clear();
    groups.clear();
    group_members.clear();
    user_groups.clear();
    messages.clear();
    notifications.clear();
    user_by_email.clear();
    courses_by_dept.clear();
    sections_by_course.clear();
    enrollments_by_student.clear();
    enrollments_by_section.clear();
}

void Database::seedDefaults() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    // 1. Hierarchy: University
    models::University univ;
    univ.id = "univ-geu-01";
    univ.name = "Graphic Era (Deemed to be University), Dehradun";
    univ.code = "GEU";
    universities[univ.id] = univ;

    // 2. Hierarchy: College
    models::College coll;
    coll.id = "coll-soc-01";
    coll.university_id = univ.id;
    coll.name = "School of Computing";
    coll.code = "SOC";
    colleges[coll.id] = coll;

    // 3. Hierarchy: Department
    models::Department dept;
    dept.id = "dept-cse-01";
    dept.college_id = coll.id;
    dept.name = "Department of Computer Science & Engineering";
    dept.code = "CSE";
    departments[dept.id] = dept;

    // 4. Users (Password: password123)
    std::string default_pwd_hash = utils::Crypto::hashPassword("password123");

    auto addUser = [&](const std::string& id, const std::string& email, const std::string& name,
                       models::UserRole role, const std::string& dept_id) {
        models::User u;
        u.id = id;
        u.email = email;
        u.full_name = name;
        u.password_hash = default_pwd_hash;
        u.role = role;
        u.department_id = dept_id;
        u.created_at = "2026-09-24 10:00:00";
        users[u.id] = u;
        user_by_email[u.email] = u.id;
    };

    addUser("user-mentor-01", "prakash.srivastava@geu.ac.in", "Prof. Dr. Prakash Srivastava", models::UserRole::INSTRUCTOR, dept.id);
    addUser("user-lead-01", "oli.mayank.0904@gmail.com", "Mayank Oli", models::UserRole::ADMIN, dept.id);
    addUser("user-std-01", "garasameera918@gmail.com", "Gara Sameera", models::UserRole::STUDENT, dept.id);
    addUser("user-std-02", "ishanisingh.ncr@gmail.com", "Ishani Singh", models::UserRole::STUDENT, dept.id);
    addUser("user-std-03", "bishtmanan205@gmail.com", "Manan Bisht", models::UserRole::STUDENT, dept.id);

    // 5. Courses
    auto addCourse = [&](const std::string& id, const std::string& code, const std::string& title, int credits, int sem) {
        models::Course c;
        c.id = id;
        c.department_id = dept.id;
        c.code = code;
        c.title = title;
        c.credits = credits;
        c.semester = sem;
        courses[c.id] = c;
        courses_by_dept[dept.id].push_back(c.id);
    };

    addCourse("course-os-01", "CS301", "Operating Systems Concepts & Concurrency", 4, 5);
    addCourse("course-dbms-01", "CS302", "Database Management Systems & Transactions", 4, 5);
    addCourse("course-ds-01", "CS201", "Data Structures & Algorithms", 3, 3);

    // 6. Sections
    auto addSection = [&](const std::string& id, const std::string& course_id, const std::string& name,
                          const std::string& instructor_id, int capacity) {
        models::Section s;
        s.id = id;
        s.course_id = course_id;
        s.section_name = name;
        s.instructor_id = instructor_id;
        s.max_capacity = capacity;
        s.current_enrolled = 0;
        s.semester = 5;
        s.year = 2026;
        sections[s.id] = s;
        sections_by_course[course_id].push_back(s.id);
    };

    addSection("sec-os-a", "course-os-01", "Section A (OS)", "user-mentor-01", 60);
    addSection("sec-dbms-b", "course-dbms-01", "Section B (DBMS)", "user-mentor-01", 60);

    // 7. Communication Groups (Auto-generated from hierarchy)
    auto addGroup = [&](const std::string& id, models::GroupType type, const std::string& target_id,
                        const std::string& name, const std::string& desc) {
        models::CommunicationGroup g;
        g.id = id;
        g.type = type;
        g.target_id = target_id;
        g.name = name;
        g.description = desc;
        groups[g.id] = g;
    };

    addGroup("grp-dept-cse", models::GroupType::DEPARTMENT, dept.id, "CSE Department General", "All CSE faculty and enrolled students");
    addGroup("grp-sec-os-a", models::GroupType::SECTION, "sec-os-a", "CS301 Section A Chat", "Operating Systems course section group");
    addGroup("grp-sec-dbms-b", models::GroupType::SECTION, "sec-dbms-b", "CS302 Section B Chat", "Database Management Systems course section group");

    // Add mentor and admin to department group
    auto enrollUserInGroup = [&](const std::string& gid, const std::string& uid) {
        group_members[gid].insert(uid);
        user_groups[uid].insert(gid);
    };

    enrollUserInGroup("grp-dept-cse", "user-mentor-01");
    enrollUserInGroup("grp-dept-cse", "user-lead-01");
    enrollUserInGroup("grp-dept-cse", "user-std-01");
    enrollUserInGroup("grp-dept-cse", "user-std-02");
    enrollUserInGroup("grp-dept-cse", "user-std-03");

    // 8. Enrollments & Section groups
    auto enrollStudent = [&](const std::string& enr_id, const std::string& student_id,
                             const std::string& section_id, const std::string& group_id) {
        models::Enrollment e;
        e.id = enr_id;
        e.student_id = student_id;
        e.section_id = section_id;
        e.status = models::EnrollmentStatus::ENROLLED;
        e.enrolled_at = "2026-09-24 10:15:00";
        enrollments[e.id] = e;
        enrollments_by_student[student_id].push_back(e.id);
        enrollments_by_section[section_id].push_back(e.id);
        sections[section_id].current_enrolled++;

        // Auto-sync into communication group!
        enrollUserInGroup(group_id, student_id);
    };

    enrollUserInGroup("grp-sec-os-a", "user-mentor-01");
    enrollStudent("enr-01", "user-std-01", "sec-os-a", "grp-sec-os-a");
    enrollStudent("enr-02", "user-std-02", "sec-os-a", "grp-sec-os-a");
    enrollStudent("enr-03", "user-std-03", "sec-os-a", "grp-sec-os-a");

    enrollUserInGroup("grp-sec-dbms-b", "user-mentor-01");
    enrollStudent("enr-04", "user-std-01", "sec-dbms-b", "grp-sec-dbms-b");
    enrollStudent("enr-05", "user-std-02", "sec-dbms-b", "grp-sec-dbms-b");
    enrollStudent("enr-06", "user-std-03", "sec-dbms-b", "grp-sec-dbms-b");

    // 9. Initial Messages
    models::Message m1;
    m1.id = "msg-01";
    m1.group_id = "grp-sec-os-a";
    m1.sender_id = "user-mentor-01";
    m1.sender_name = "Prof. Dr. Prakash Srivastava";
    m1.content = "Welcome to CS301 Operating Systems! Concurrency Engine is ready.";
    m1.type = models::MessageType::ANNOUNCEMENT;
    m1.priority = 5;
    m1.timestamp = "2026-09-24 10:30:00";
    messages.push_back(m1);

    models::Message m2;
    m2.id = "msg-02";
    m2.group_id = "grp-sec-os-a";
    m2.sender_id = "user-lead-01";
    m2.sender_name = "Mayank Oli";
    m2.content = "Producer-consumer queue and CPU scheduling active with FCFS, SJF, and Round Robin.";
    m2.type = models::MessageType::TEXT;
    m2.priority = 1;
    m2.timestamp = "2026-09-24 10:35:00";
    messages.push_back(m2);

    utils::Logger::getInstance().info("Database", "Database initialized and seeded successfully with Graphic Era data.");
}

} // namespace database
} // namespace uniconnect
