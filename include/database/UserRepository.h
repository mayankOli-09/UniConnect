#pragma once

#include <string>
#include <vector>
#include <optional>
#include "models/User.h"
#include "database/Database.h"

namespace uniconnect {
namespace database {

class UserRepository {
public:
    explicit UserRepository(Database& db = Database::getInstance());

    std::optional<models::User> findById(const std::string& id);
    std::optional<models::User> findByEmail(const std::string& email);
    std::vector<models::User> findAll();
    std::vector<models::User> findByRole(models::UserRole role);
    std::vector<models::User> findByDepartment(const std::string& dept_id);

    bool save(const models::User& user);
    bool remove(const std::string& id);

private:
    Database& db_;
};

} // namespace database
} // namespace uniconnect
