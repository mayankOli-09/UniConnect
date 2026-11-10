#pragma once

#include <string>
#include <optional>
#include "database/UserRepository.h"
#include "models/User.h"
#include "utils/Json.h"

namespace uniconnect {
namespace services {

struct AuthResult {
    bool success{false};
    std::string token;
    models::User user;
    std::string error_message;

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["success"] = success;
        if (success) {
            obj["token"] = token;
            obj["user"] = user.toJson();
        } else {
            obj["error"] = error_message;
        }
        return obj;
    }
};

class AuthService {
public:
    explicit AuthService(database::UserRepository& user_repo,
                         std::string jwt_secret = "uniconnect_jwt_secret_key_2026");

    AuthResult login(const std::string& email, const std::string& password);
    AuthResult registerUser(const std::string& name,
                            const std::string& email,
                            const std::string& password,
                            models::UserRole role,
                            const std::string& dept_id);

    bool validateToken(const std::string& token, std::string& out_user_id, models::UserRole& out_role);

private:
    database::UserRepository& user_repo_;
    std::string jwt_secret_;
};

} // namespace services
} // namespace uniconnect
