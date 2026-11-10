#include "services/AuthService.h"
#include "utils/Crypto.h"
#include "utils/UUID.h"
#include "utils/Logger.h"
#include <chrono>

namespace uniconnect {
namespace services {

AuthService::AuthService(database::UserRepository& user_repo, std::string jwt_secret)
    : user_repo_(user_repo), jwt_secret_(std::move(jwt_secret)) {}

AuthResult AuthService::login(const std::string& email, const std::string& password) {
    AuthResult res;
    auto opt_user = user_repo_.findByEmail(email);
    if (!opt_user.has_value()) {
        res.success = false;
        res.error_message = "Invalid email or user not found";
        return res;
    }

    const auto& user = opt_user.value();
    if (!utils::Crypto::verifyPassword(password, user.password_hash)) {
        res.success = false;
        res.error_message = "Invalid password";
        return res;
    }

    // Build JWT payload
    auto payload = utils::JsonValue::object();
    payload["sub"] = user.id;
    payload["email"] = user.email;
    payload["role"] = models::userRoleToString(user.role);
    auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    payload["iat"] = now_sec;
    payload["exp"] = now_sec + 86400 * 7; // 7 days

    std::string token = utils::Crypto::createJWT(payload.serialize(false), jwt_secret_);
    res.success = true;
    res.token = token;
    res.user = user;
    utils::Logger::getInstance().info("Auth", "User logged in:", user.email, "role:", models::userRoleToString(user.role));
    return res;
}

AuthResult AuthService::registerUser(const std::string& name,
                                     const std::string& email,
                                     const std::string& password,
                                     models::UserRole role,
                                     const std::string& dept_id) {
    AuthResult res;
    if (user_repo_.findByEmail(email).has_value()) {
        res.success = false;
        res.error_message = "Email already registered";
        return res;
    }

    models::User u;
    u.id = "user-" + utils::UUID::generate().substr(0, 8);
    u.email = email;
    u.full_name = name;
    u.password_hash = utils::Crypto::hashPassword(password);
    u.role = role;
    u.department_id = dept_id;
    u.created_at = "2026-09-24 12:00:00";

    user_repo_.save(u);
    return login(email, password);
}

bool AuthService::validateToken(const std::string& token, std::string& out_user_id, models::UserRole& out_role) {
    std::string payload_str;
    if (!utils::Crypto::verifyJWT(token, jwt_secret_, payload_str)) {
        return false;
    }

    utils::JsonValue payload;
    if (!utils::JsonValue::tryParse(payload_str, payload)) {
        return false;
    }

    out_user_id = payload["sub"].asString();
    out_role = models::stringToUserRole(payload["role"].asString());

    auto exp = payload["exp"].asInt64();
    auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (exp > 0 && now_sec > exp) {
        return false; // Expired
    }

    return !out_user_id.empty();
}

} // namespace services
} // namespace uniconnect
