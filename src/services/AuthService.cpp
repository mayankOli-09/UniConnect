#include "services/AuthService.h"

#include <cctype>
#include <ctime>
#include <iterator>

#include "auth/PasswordHasher.h"
#include "utils/Crypto.h"
#include "utils/Env.h"
#include "utils/Logger.h"

namespace uniconnect {
namespace services {

namespace {

const char* kBadLogin = "Invalid email or password.";

long long nowSeconds() { return static_cast<long long>(std::time(nullptr)); }

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool validEmail(const std::string& e) {
    if (e.size() < 3 || e.size() > 150) return false;
    const size_t at = e.find('@');
    if (at == std::string::npos || at != e.rfind('@') || at == 0 || at > 64) return false;
    const std::string local = e.substr(0, at), domain = e.substr(at + 1);
    for (char c : local)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '%' || c == '+' || c == '-')) return false;
    if (domain.size() < 3 || domain.find('.') == std::string::npos) return false;
    if (domain.front() == '.' || domain.back() == '.' || domain.front() == '-' || domain.find("..") != std::string::npos) return false;
    for (char c : domain)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-')) return false;
    return true;
}

bool parsePositiveInt(const std::string& s, int& out) {
    if (s.empty() || s.size() > 9) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    out = std::atoi(s.c_str());
    return out > 0;
}

AuthResult fail(const std::string& msg, int status = 400) {
    AuthResult r;
    r.status = status;
    r.error_message = msg;
    return r;
}

} // namespace

AuthConfig AuthConfig::fromEnvironment() {
    AuthConfig c;
    c.jwt_secret = utils::getEnvVar("JWT_SECRET");
    const std::string hours = utils::getEnvVar("TOKEN_TTL_HOURS");
    int h = 0;
    if (parsePositiveInt(hours, h) && h <= 24 * 30) c.token_ttl_seconds = static_cast<long long>(h) * 3600;
    c.allow_privileged_signup = utils::getEnvVar("UNICONNECT_ALLOW_PRIVILEGED_SIGNUP") == "1";
    return c;
}

utils::JsonValue AuthResult::toJson() const {
    auto o = utils::JsonValue::object();
    o["success"] = success;
    o["token"] = token;
    o["token_type"] = "Bearer";
    o["expires_in"] = expires_in;
    o["user"] = user.toJson();
    return o;
}

AuthService::AuthService(database::UserRepository& repo, AuthConfig config)
    : repo_(repo), config_(std::move(config)), jwt_(config_.jwt_secret.empty() ? "unset" : config_.jwt_secret) {
    auto& log = utils::Logger::getInstance();
    if (config_.jwt_secret.empty()) {
        // Never fall back to a guessable secret: use a random one (tokens stop working after a restart).
        config_.jwt_secret = utils::crypto::toHex(utils::crypto::randomBytes(32));
        jwt_ = auth::JwtService(config_.jwt_secret);
        log.warn("Auth", "JWT_SECRET is not set: using a random secret for this run. Users must sign in again after a restart.");
    } else if (config_.jwt_secret.size() < 16) {
        log.warn("Auth", "JWT_SECRET is shorter than 16 characters; use a longer secret.");
    }
    if (config_.allow_privileged_signup)
        log.warn("Auth", "Anyone may self-register as FACULTY or ADMIN (UNICONNECT_ALLOW_PRIVILEGED_SIGNUP=1).");
    dummy_hash_ = auth::PasswordHasher::hash("dummy-password-for-timing");
}

AuthResult AuthService::issue(const models::User& u) const {
    AuthResult r;
    r.success = true;
    r.user = u;
    r.user.password_hash.clear();
    r.expires_in = config_.token_ttl_seconds;
    r.token = jwt_.issue(u.id, models::userRoleToString(u.role), config_.token_ttl_seconds);
    return r;
}

std::optional<auth::JwtClaims> AuthService::verifyToken(const std::string& token) const {
    auth::JwtClaims c;
    if (!jwt_.verify(token, c)) return std::nullopt;
    return c;
}

long long AuthService::secondsLocked(const std::string& email) {
    std::lock_guard<std::mutex> l(attempts_mutex_);
    auto it = attempts_.find(email);
    if (it == attempts_.end()) return 0;
    const long long now = nowSeconds();
    if (it->second.locked_until > now) return it->second.locked_until - now;
    if (it->second.locked_until != 0) attempts_.erase(it);   // lock expired
    return 0;
}

void AuthService::recordFailure(const std::string& email) {
    std::lock_guard<std::mutex> l(attempts_mutex_);
    if (attempts_.size() > 5000) {                       // bound memory use
        const long long now = nowSeconds();
        for (auto it = attempts_.begin(); it != attempts_.end();)
            it = (it->second.locked_until != 0 && it->second.locked_until <= now) ? attempts_.erase(it) : std::next(it);
        if (attempts_.size() > 5000) return;
    }
    auto& a = attempts_[email];
    if (++a.failures >= config_.max_failed_logins) {
        a.locked_until = nowSeconds() + config_.lockout_seconds;
        a.failures = 0;
    }
}

void AuthService::clearFailures(const std::string& email) {
    std::lock_guard<std::mutex> l(attempts_mutex_);
    attempts_.erase(email);
}

AuthResult AuthService::login(const std::string& rawEmail, const std::string& password) {
    const std::string email = lower(trim(rawEmail));
    if (email.empty() || password.empty()) return fail("Email and password are required.");
    if (email.size() > 150 || password.size() > 128) return fail(kBadLogin, 401);

    if (const long long wait = secondsLocked(email); wait > 0)
        return fail("Too many failed attempts. Try again in " + std::to_string((wait + 59) / 60) + " minute(s).", 429);

    const auto user = repo_.findByEmail(email);
    // Always do one hash verification, whether or not the account exists.
    const bool ok = auth::PasswordHasher::verify(password, user ? user->password_hash : dummy_hash_);
    if (!user && !repo_.healthy()) return fail("The service is temporarily unavailable. Please try again shortly.", 503);
    if (!user || !ok) {
        recordFailure(email);
        return fail(kBadLogin, 401);
    }
    clearFailures(email);
    return issue(*user);
}

AuthResult AuthService::registerUser(const std::string& rawName, const std::string& rawEmail, const std::string& password,
                                     models::UserRole role, const std::string& department_id,
                                     const std::string& section_id) {
    if (role != models::UserRole::STUDENT && !config_.allow_privileged_signup)
        return fail("Faculty and Admin accounts are created by an administrator. Please register as a Student.", 403);

    const std::string name = trim(rawName);
    const std::string email = lower(trim(rawEmail));
    if (name.size() < 2 || name.size() > 100) return fail("Name must be between 2 and 100 characters.");
    for (unsigned char c : name) if (c < 0x20 || c == 0x7f) return fail("Name contains invalid characters.");
    if (!validEmail(email)) return fail("Please enter a valid email address.");
    if (password.size() < 8) return fail("Password must be at least 8 characters.");
    if (password.size() > 128) return fail("Password must be at most 128 characters.");

    int dept = 0;
    if (!parsePositiveInt(trim(department_id), dept) || !repo_.departmentExists(dept))
        return fail("Please choose a valid department.");

    database::NewUser nu;
    nu.name = name;
    nu.email = email;
    nu.role = role;
    nu.department_id = dept;
    if (role == models::UserRole::STUDENT && !trim(section_id).empty()) {
        int sec = 0;
        if (!parsePositiveInt(trim(section_id), sec)) return fail("Invalid section.");
        nu.section_id = sec;
    }
    nu.password_hash = auth::PasswordHasher::hash(password);

    const auto res = repo_.createUser(nu);
    if (!res.ok) return fail(res.error.empty() ? "Could not create the account." : res.error, res.status);
    return issue(res.user);
}

} // namespace services
} // namespace uniconnect
