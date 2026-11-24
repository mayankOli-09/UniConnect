#pragma once
#include <string>

#include "database/AcademicRepository.h"
#include "models/User.h"
#include "services/ServiceResult.h"
#include "utils/Json.h"

namespace uniconnect {
namespace services {

class AcademicService {
public:
    explicit AcademicService(database::AcademicRepository& repo) : repo_(repo) {}

    utils::JsonValue courses();     // {"courses":[...]}
    utils::JsonValue hierarchy();   // university -> departments -> programs -> sections -> courses

    // The caller is the signed-in user. Students may only change their own enrollment; admins may change anyone's.
    ServiceResult enroll(const models::User& caller, const std::string& student_id, const std::string& section_id, const std::string& course_id);
    ServiceResult drop(const models::User& caller, const std::string& student_id, const std::string& section_id, const std::string& course_id);

    // Pure function, exposed for tests.
    static utils::JsonValue buildHierarchy(const database::AcademicData& data);

private:
    ServiceResult change(bool enroll, const models::User& caller, const std::string& student_id, const std::string& section_id, const std::string& course_id);
    database::AcademicRepository& repo_;
};

} // namespace services
} // namespace uniconnect
