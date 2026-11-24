#pragma once
#include <string>

namespace uniconnect {
namespace auth {

struct JwtClaims {
    int user_id = 0;
    std::string role;
    long long issued_at = 0;
    long long expires_at = 0;
};

// HS256 JSON Web Tokens.
class JwtService {
public:
    explicit JwtService(std::string secret) : secret_(std::move(secret)) {}

    std::string issue(int user_id, const std::string& role, long long ttl_seconds) const;
    // True only for a well-formed HS256 token with a valid signature that has not expired.
    bool verify(const std::string& token, JwtClaims& out) const;
    // Signs an arbitrary payload with the standard HS256 header (used by tests).
    std::string signPayload(const std::string& payload_json) const;

private:
    std::string secret_;
};

} // namespace auth
} // namespace uniconnect
