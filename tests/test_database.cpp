#include "database/Database.h"
#include "database/UserRepository.h"
#include "database/CourseRepository.h"
#include "database/EnrollmentRepository.h"
#include "database/MessageRepository.h"
#include <iostream>
#include <cassert>

using namespace uniconnect;

void testHierarchyAndUsers() {
    std::cout << "[TestDatabase] Testing Hierarchy and User Repository..." << std::endl;
    auto& db = database::Database::getInstance();
    database::UserRepository user_repo(db);
    database::CourseRepository course_repo(db);

    auto univs = course_repo.getUniversities();
    assert(!univs.empty());
    assert(univs[0].code == "GEU");

    auto mentor = user_repo.findByEmail("prakash.srivastava@geu.ac.in");
    assert(mentor.has_value());
    assert(mentor->role == models::UserRole::INSTRUCTOR);

    auto students = user_repo.findByRole(models::UserRole::STUDENT);
    assert(students.size() >= 3);
    std::cout << " -> PASSED" << std::endl;
}

void testAutomaticGroupSync() {
    std::cout << "[TestDatabase] Testing Automatic Communication-Group Synchronization..." << std::endl;
    auto& db = database::Database::getInstance();
    database::EnrollmentRepository enroll_repo(db);

    std::string test_student_id = "user-test-99";
    std::string test_section_id = "sec-os-a";
    std::string target_group_id = "grp-sec-os-a";

    // 1. Verify student is not in section group initially
    auto initial_members = enroll_repo.getGroupMembers(target_group_id);
    assert(initial_members.find(test_student_id) == initial_members.end());

    // 2. Enroll student into Section
    models::Enrollment enr;
    bool enrolled = enroll_repo.enroll(test_student_id, test_section_id, enr);
    assert(enrolled);
    assert(enroll_repo.isEnrolled(test_student_id, test_section_id));

    // 3. Verify student is AUTOMATICALLY synchronized into the section's communication group!
    auto updated_members = enroll_repo.getGroupMembers(target_group_id);
    assert(updated_members.find(test_student_id) != updated_members.end());
    std::cout << " -> Auto-enroll group sync verified!" << std::endl;

    // 4. Drop course and verify student is AUTOMATICALLY removed from the group!
    bool dropped = enroll_repo.drop(test_student_id, test_section_id);
    assert(dropped);

    auto final_members = enroll_repo.getGroupMembers(target_group_id);
    assert(final_members.find(test_student_id) == final_members.end());
    std::cout << " -> Auto-drop group sync verified!" << std::endl;
    std::cout << " -> PASSED" << std::endl;
}

void testMessages() {
    std::cout << "[TestDatabase] Testing Message Repository..." << std::endl;
    auto& db = database::Database::getInstance();
    database::MessageRepository msg_repo(db);

    models::Message msg;
    msg.id = "test-msg-1";
    msg.group_id = "grp-sec-os-a";
    msg.sender_id = "user-lead-01";
    msg.sender_name = "Mayank Oli";
    msg.content = "Test message for DBMS persistence";
    msg.type = models::MessageType::TEXT;
    msg.priority = 1;
    msg.timestamp = "2026-09-24 14:00:00";

    assert(msg_repo.saveMessage(msg));

    auto msgs = msg_repo.getMessagesByGroup("grp-sec-os-a", 10);
    assert(!msgs.empty());
    assert(msgs.back().id == "test-msg-1");
    std::cout << " -> PASSED" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "      UniConnect Database Tests         " << std::endl;
    std::cout << "========================================" << std::endl;

    testHierarchyAndUsers();
    testAutomaticGroupSync();
    testMessages();

    std::cout << "All Database tests PASSED!" << std::endl;
    return 0;
}
