#include "services/UserService.h"

namespace uniconnect {
namespace services {

UserService::UserService(database::UserRepository& repo) : repo_(repo) {}

std::optional<models::User> UserService::getUserById(const std::string& id) {
    return repo_.findById(id);
}

std::optional<models::User> UserService::getUserByEmail(const std::string& email) {
    return repo_.findByEmail(email);
}

std::vector<models::User> UserService::getAllUsers() {
    return repo_.findAll();
}

std::vector<models::User> UserService::getUsersByRole(models::UserRole role) {
    return repo_.findByRole(role);
}

std::vector<models::User> UserService::getUsersByDepartment(const std::string& dept_id) {
    return repo_.findByDepartment(dept_id);
}

} // namespace services
} // namespace uniconnect
