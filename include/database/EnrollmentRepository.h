#pragma once

#include <string>
#include <vector>
#include <set>
#include <optional>
#include "models/Section.h"
#include "models/Message.h"
#include "database/Database.h"

namespace uniconnect {
namespace database {

class EnrollmentRepository {
public:
    explicit EnrollmentRepository(Database& db = Database::getInstance());

    // Enrollment actions
    bool enroll(const std::string& student_id, const std::string& section_id, models::Enrollment& out_enrollment);
    bool drop(const std::string& student_id, const std::string& section_id);

    std::vector<models::Enrollment> findByStudent(const std::string& student_id);
    std::vector<models::Enrollment> findBySection(const std::string& section_id);
    bool isEnrolled(const std::string& student_id, const std::string& section_id);

    // Automatic group membership synchronizations
    models::CommunicationGroup getOrCreateGroup(models::GroupType type,
                                                const std::string& target_id,
                                                const std::string& name,
                                                const std::string& desc = "");

    void syncUserToGroup(const std::string& group_id, const std::string& user_id, bool add);
    std::vector<models::CommunicationGroup> getUserGroups(const std::string& user_id);
    std::set<std::string> getGroupMembers(const std::string& group_id);
    std::optional<models::CommunicationGroup> findGroupById(const std::string& group_id);
    std::optional<models::CommunicationGroup> findGroupByTargetId(const std::string& target_id);

private:
    Database& db_;
};

} // namespace database
} // namespace uniconnect
