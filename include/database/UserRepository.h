#pragma once
#include <optional>
#include <string>

#include "models/User.h"

namespace uniconnect {
namespace database {

struct NewUser {
    std::string name;
    std::string email;          // already normalised (trimmed, lower-case)
    std::string password_hash;
    models::UserRole role = models::UserRole::STUDENT;
    int department_id = 0;
    std::optional<int> section_id;   // students only; when empty the repository picks the department's first section
};

struct CreateUserResult {
    bool ok = false;
    std::string error;          // user-presentable reason when !ok
    int status = 400;           // suggested HTTP status when !ok (409 duplicate, 503 unavailable, 500 other)
    models::User user;
};

// Storage for users. Implemented by MySqlUserRepository (real database) and InMemoryUserRepository (tests).
class UserRepository {
public:
    virtual ~UserRepository() = default;
    virtual std::optional<models::User> findByEmail(const std::string& email) = 0;
    virtual std::optional<models::User> findById(int id) = 0;
    virtual bool departmentExists(int department_id) = 0;
    virtual bool healthy() { return true; }   // false when the storage cannot be reached
    // Must create the users row and the students/faculty row atomically.
    virtual CreateUserResult createUser(const NewUser& u) = 0;
};

} // namespace database
} // namespace uniconnect
