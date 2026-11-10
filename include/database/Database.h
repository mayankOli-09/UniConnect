#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <memory>
#include "models/User.h"
#include "models/Course.h"
#include "models/Section.h"
#include "models/Message.h"
#include "models/Notification.h"

namespace uniconnect {
namespace database {

class Database {
public:
    static Database& getInstance();

    void seedDefaults();
    void clear();

    // Direct accessors protected by internal mutex
    std::mutex& getMutex() { return db_mutex_; }

    // Table storage
    std::map<std::string, models::University> universities;
    std::map<std::string, models::College> colleges;
    std::map<std::string, models::Department> departments;
    std::map<std::string, models::User> users;
    std::map<std::string, models::Course> courses;
    std::map<std::string, models::Section> sections;
    std::map<std::string, models::Enrollment> enrollments;
    std::map<std::string, models::CommunicationGroup> groups;
    std::map<std::string, std::set<std::string>> group_members; // group_id -> set of user_id
    std::map<std::string, std::set<std::string>> user_groups;   // user_id -> set of group_id
    std::vector<models::Message> messages;
    std::vector<models::Notification> notifications;

    // Fast Secondary Indexes
    std::map<std::string, std::string> user_by_email;
    std::map<std::string, std::vector<std::string>> courses_by_dept;
    std::map<std::string, std::vector<std::string>> sections_by_course;
    std::map<std::string, std::vector<std::string>> enrollments_by_student;
    std::map<std::string, std::vector<std::string>> enrollments_by_section;

private:
    Database();
    std::mutex db_mutex_;
};

} // namespace database
} // namespace uniconnect
