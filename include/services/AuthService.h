#pragma once
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include "auth/JwtService.h"
#include "database/UserRepository.h"
#include "models/User.h"
#include "utils/Json.h"

namespace uniconnect {
namespace services {

struct AuthConfig {
    std::string jwt_secret;                  // empty => a random secret is generated for this run
    long long token_ttl_seconds = 24 * 3600;
    bool allow_privileged_signup = false;    // may anyone self-register as FACULTY / ADMIN?
    int max_failed_logins = 5;               // per email ...
    int lockout_seconds = 15 * 60;           // ... then locked for this long

    // JWT_SECRET, TOKEN_TTL_HOURS, UNICONNECT_ALLOW_PRIVILEGED_SIGNUP=1
    static AuthConfig fromEnvironment();
};

struct AuthResult {
    bool success = false;
    int status = 200;                // suggested HTTP status (401 bad login, 409 duplicate, 429 locked out, ...)
    std::string error_message;
    std::string token;
    long long expires_in = 0;
    models::User user;

    utils::JsonValue toJson() const;
};

class AuthService {
public:
    AuthService(database::UserRepository& repo, AuthConfig config = AuthConfig::fromEnvironment());

    AuthResult login(const std::string& email, const std::string& password);
    // department_id / section_id arrive as text (JSON numbers are read as text too).
    AuthResult registerUser(const std::string& name, const std::string& email, const std::string& password,
                            models::UserRole role, const std::string& department_id,
                            const std::string& section_id = "");

    // For the WebSocket handshake and any authenticated route.
    std::optional<auth::JwtClaims> verifyToken(const std::string& token) const;

private:
    struct Attempts { int failures = 0; long long locked_until = 0; };

    long long secondsLocked(const std::string& email);
    void recordFailure(const std::string& email);
    void clearFailures(const std::string& email);
    AuthResult issue(const models::User& u) const;

    database::UserRepository& repo_;
    AuthConfig config_;
    auth::JwtService jwt_;
    std::mutex attempts_mutex_;
    std::map<std::string, Attempts> attempts_;
    std::string dummy_hash_;   // verified against when the email is unknown, so timing does not reveal it
};

} // namespace services
} // namespace uniconnect
