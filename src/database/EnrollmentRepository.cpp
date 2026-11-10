#include "database/EnrollmentRepository.h"
#include "utils/UUID.h"
#include "utils/Logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace uniconnect {
namespace database {

EnrollmentRepository::EnrollmentRepository(Database& db) : db_(db) {}

static std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt{};
#if defined(_WIN32)
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif
    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool EnrollmentRepository::enroll(const std::string& student_id, const std::string& section_id, models::Enrollment& out_enrollment) {
    std::lock_guard<std::mutex> lock(db_.getMutex());

    auto sit = db_.sections.find(section_id);
    if (sit == db_.sections.end()) {
        return false;
    }

    if (sit->second.current_enrolled >= sit->second.max_capacity) {
        return false; // Section is full
    }

    // Check if already actively enrolled
    for (const auto& [_, enr] : db_.enrollments) {
        if (enr.student_id == student_id && enr.section_id == section_id && enr.status == models::EnrollmentStatus::ENROLLED) {
            out_enrollment = enr;
            return true;
        }
    }

    models::Enrollment enr;
    enr.id = "enr-" + utils::UUID::generate().substr(0, 8);
    enr.student_id = student_id;
    enr.section_id = section_id;
    enr.status = models::EnrollmentStatus::ENROLLED;
    enr.enrolled_at = currentTimestamp();

    db_.enrollments[enr.id] = enr;
    db_.enrollments_by_student[student_id].push_back(enr.id);
    db_.enrollments_by_section[section_id].push_back(enr.id);
    sit->second.current_enrolled++;

    // Automatic Group Membership Synchronization
    std::string group_id;
    for (const auto& [gid, grp] : db_.groups) {
        if (grp.type == models::GroupType::SECTION && grp.target_id == section_id) {
            group_id = gid;
            break;
        }
    }

    if (group_id.empty()) {
        // Auto-create communication group for section
        models::CommunicationGroup grp;
        grp.id = "grp-" + section_id;
        grp.type = models::GroupType::SECTION;
        grp.target_id = section_id;
        grp.name = sit->second.section_name + " Chat";
        grp.description = "Auto-generated section communication group";
        db_.groups[grp.id] = grp;
        group_id = grp.id;
    }

    db_.group_members[group_id].insert(student_id);
    db_.user_groups[student_id].insert(group_id);

    // Also ensure student is in department group
    auto uit = db_.users.find(student_id);
    if (uit != db_.users.end() && !uit->second.department_id.empty()) {
        for (const auto& [dgid, dgrp] : db_.groups) {
            if (dgrp.type == models::GroupType::DEPARTMENT && dgrp.target_id == uit->second.department_id) {
                db_.group_members[dgid].insert(student_id);
                db_.user_groups[student_id].insert(dgid);
            }
        }
    }

    out_enrollment = enr;
    utils::Logger::getInstance().info("Enrollment", "Student", student_id, "enrolled in section", section_id, "-> group sync:", group_id);
    return true;
}

bool EnrollmentRepository::drop(const std::string& student_id, const std::string& section_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());

    bool found = false;
    for (auto& [_, enr] : db_.enrollments) {
        if (enr.student_id == student_id && enr.section_id == section_id && enr.status == models::EnrollmentStatus::ENROLLED) {
            enr.status = models::EnrollmentStatus::DROPPED;
            found = true;
            break;
        }
    }

    if (!found) {
        return false;
    }

    auto sit = db_.sections.find(section_id);
    if (sit != db_.sections.end() && sit->second.current_enrolled > 0) {
        sit->second.current_enrolled--;
    }

    // Auto-remove student from section communication group!
    for (const auto& [gid, grp] : db_.groups) {
        if (grp.type == models::GroupType::SECTION && grp.target_id == section_id) {
            db_.group_members[gid].erase(student_id);
            db_.user_groups[student_id].erase(gid);
            utils::Logger::getInstance().info("Enrollment", "Student", student_id, "dropped section", section_id, "-> removed from group:", gid);
            break;
        }
    }

    return true;
}

std::vector<models::Enrollment> EnrollmentRepository::findByStudent(const std::string& student_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::Enrollment> list;
    auto it = db_.enrollments_by_student.find(student_id);
    if (it != db_.enrollments_by_student.end()) {
        for (const auto& eid : it->second) {
            auto eit = db_.enrollments.find(eid);
            if (eit != db_.enrollments.end()) list.push_back(eit->second);
        }
    }
    return list;
}

std::vector<models::Enrollment> EnrollmentRepository::findBySection(const std::string& section_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::Enrollment> list;
    auto it = db_.enrollments_by_section.find(section_id);
    if (it != db_.enrollments_by_section.end()) {
        for (const auto& eid : it->second) {
            auto eit = db_.enrollments.find(eid);
            if (eit != db_.enrollments.end()) list.push_back(eit->second);
        }
    }
    return list;
}

bool EnrollmentRepository::isEnrolled(const std::string& student_id, const std::string& section_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    for (const auto& [_, enr] : db_.enrollments) {
        if (enr.student_id == student_id && enr.section_id == section_id && enr.status == models::EnrollmentStatus::ENROLLED) {
            return true;
        }
    }
    return false;
}

models::CommunicationGroup EnrollmentRepository::getOrCreateGroup(models::GroupType type,
                                                                  const std::string& target_id,
                                                                  const std::string& name,
                                                                  const std::string& desc) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    for (const auto& [_, grp] : db_.groups) {
        if (grp.type == type && grp.target_id == target_id) {
            return grp;
        }
    }

    models::CommunicationGroup g;
    g.id = "grp-" + target_id;
    g.type = type;
    g.target_id = target_id;
    g.name = name;
    g.description = desc;
    db_.groups[g.id] = g;
    return g;
}

void EnrollmentRepository::syncUserToGroup(const std::string& group_id, const std::string& user_id, bool add) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    if (add) {
        db_.group_members[group_id].insert(user_id);
        db_.user_groups[user_id].insert(group_id);
    } else {
        db_.group_members[group_id].erase(user_id);
        db_.user_groups[user_id].erase(group_id);
    }
}

std::vector<models::CommunicationGroup> EnrollmentRepository::getUserGroups(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::CommunicationGroup> list;
    auto it = db_.user_groups.find(user_id);
    if (it != db_.user_groups.end()) {
        for (const auto& gid : it->second) {
            auto git = db_.groups.find(gid);
            if (git != db_.groups.end()) list.push_back(git->second);
        }
    }
    return list;
}

std::set<std::string> EnrollmentRepository::getGroupMembers(const std::string& group_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    auto it = db_.group_members.find(group_id);
    return it != db_.group_members.end() ? it->second : std::set<std::string>{};
}

std::optional<models::CommunicationGroup> EnrollmentRepository::findGroupById(const std::string& group_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    auto it = db_.groups.find(group_id);
    if (it != db_.groups.end()) return it->second;
    return std::nullopt;
}

std::optional<models::CommunicationGroup> EnrollmentRepository::findGroupByTargetId(const std::string& target_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    for (const auto& [_, g] : db_.groups) {
        if (g.target_id == target_id) return g;
    }
    return std::nullopt;
}

} // namespace database
} // namespace uniconnect
