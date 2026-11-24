#pragma once
#include "database/MySqlDatabase.h"
#include "database/UserRepository.h"

namespace uniconnect {
namespace database {

// users + students / faculty / admins, using the tables from database/uniconnect.sql.
class MySqlUserRepository : public UserRepository {
public:
    explicit MySqlUserRepository(MySqlDatabase& db) : db_(db) {}

    std::optional<models::User> findByEmail(const std::string& email) override;
    std::optional<models::User> findById(int id) override;
    bool departmentExists(int department_id) override;
    bool healthy() override;
    CreateUserResult createUser(const NewUser& u) override;

private:
    std::optional<models::User> findOne(const std::string& where, const DbValue& value);
    MySqlDatabase& db_;
};

} // namespace database
} // namespace uniconnect
