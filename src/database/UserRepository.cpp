#include "database/UserRepository.h"

namespace uniconnect {
namespace database {

UserRepository::UserRepository(Database& db) : db_(db) {}

std::optional<models::User> UserRepository::findById(const std::string& id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    auto it = db_.users.find(id);
    if (it != db_.users.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<models::User> UserRepository::findByEmail(const std::string& email) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    auto it = db_.user_by_email.find(email);
    if (it != db_.user_by_email.end()) {
        auto uit = db_.users.find(it->second);
        if (uit != db_.users.end()) {
            return uit->second;
        }
    }
    return std::nullopt;
}

std::vector<models::User> UserRepository::findAll() {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::User> result;
    result.reserve(db_.users.size());
    for (const auto& [_, u] : db_.users) {
        result.push_back(u);
    }
    return result;
}

std::vector<models::User> UserRepository::findByRole(models::UserRole role) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::User> result;
    for (const auto& [_, u] : db_.users) {
        if (u.role == role) {
            result.push_back(u);
        }
    }
    return result;
}

std::vector<models::User> UserRepository::findByDepartment(const std::string& dept_id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<models::User> result;
    for (const auto& [_, u] : db_.users) {
        if (u.department_id == dept_id) {
            result.push_back(u);
        }
    }
    return result;
}

bool UserRepository::save(const models::User& user) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    db_.users[user.id] = user;
    db_.user_by_email[user.email] = user.id;
    return true;
}

bool UserRepository::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    auto it = db_.users.find(id);
    if (it != db_.users.end()) {
        db_.user_by_email.erase(it->second.email);
        db_.users.erase(it);
        return true;
    }
    return false;
}

} // namespace database
} // namespace uniconnect
