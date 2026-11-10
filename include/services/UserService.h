#pragma once

#include <string>
#include <vector>
#include <optional>
#include "database/UserRepository.h"
#include "models/User.h"
#include "utils/Json.h"

namespace uniconnect {
namespace services {

class UserService {
public:
    explicit UserService(database::UserRepository& repo);

    std::optional<models::User> getUserById(const std::string& id);
    std::optional<models::User> getUserByEmail(const std::string& email);
    std::vector<models::User> getAllUsers();
    std::vector<models::User> getUsersByRole(models::UserRole role);
    std::vector<models::User> getUsersByDepartment(const std::string& dept_id);

private:
    database::UserRepository& repo_;
};

} // namespace services
} // namespace uniconnect
