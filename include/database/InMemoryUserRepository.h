#pragma once
// Non-persistent UserRepository used by the automated tests.
#include <map>
#include <mutex>
#include <vector>

#include "database/UserRepository.h"

namespace uniconnect {
namespace database {

class InMemoryUserRepository : public UserRepository {
public:
    // department id -> first section id (mirrors the seeded database: 1->1, 2->3, 3->4)
    explicit InMemoryUserRepository(std::map<int, int> dept_first_section = {{1, 1}, {2, 3}, {3, 4}})
        : sections_(std::move(dept_first_section)) {}

    std::optional<models::User> findByEmail(const std::string& email) override {
        std::lock_guard<std::mutex> l(m_);
        for (auto& u : users_) if (u.email == email) return u;
        return std::nullopt;
    }
    std::optional<models::User> findById(int id) override {
        std::lock_guard<std::mutex> l(m_);
        for (auto& u : users_) if (u.id == id) return u;
        return std::nullopt;
    }
    bool departmentExists(int d) override {
        std::lock_guard<std::mutex> l(m_);
        return sections_.count(d) > 0;
    }
    CreateUserResult createUser(const NewUser& n) override {
        std::lock_guard<std::mutex> l(m_);
        CreateUserResult r;
        for (auto& u : users_) if (u.email == n.email) { r.error = "An account with this email already exists."; r.status = 409; return r; }
        models::User u;
        u.id = next_user_++;
        u.name = n.name; u.email = n.email; u.password_hash = n.password_hash; u.role = n.role;
        u.department_id = n.department_id;
        if (n.role == models::UserRole::STUDENT) {
            auto it = sections_.find(n.department_id);
            if (!n.section_id && it == sections_.end()) { r.error = "No section is available for this department."; return r; }
            u.section_id = n.section_id ? *n.section_id : it->second;
            u.student_id = next_student_++;
        } else if (n.role == models::UserRole::FACULTY) {
            u.faculty_id = next_faculty_++;
        }
        users_.push_back(u);
        r.ok = true; r.user = u;
        return r;
    }

private:
    std::mutex m_;
    std::vector<models::User> users_;
    std::map<int, int> sections_;
    int next_user_ = 1, next_student_ = 1, next_faculty_ = 1;
};

} // namespace database
} // namespace uniconnect
