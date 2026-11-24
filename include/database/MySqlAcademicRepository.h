#pragma once
#include "database/AcademicRepository.h"
#include "database/MySqlDatabase.h"

namespace uniconnect {
namespace database {

class MySqlAcademicRepository : public AcademicRepository {
public:
    explicit MySqlAcademicRepository(MySqlDatabase& db) : db_(db) {}
    AcademicData loadAcademicData() override;
    EnrollResult enroll(int student_id, int section_id, std::optional<int> course_id) override;
    EnrollResult drop(int student_id, int section_id, std::optional<int> course_id) override;

private:
    MySqlDatabase& db_;
};

} // namespace database
} // namespace uniconnect
