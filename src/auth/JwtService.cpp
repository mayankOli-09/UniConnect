#include "auth/JwtService.h"

#include <ctime>

#include "utils/Crypto.h"
#include "utils/Json.h"

namespace uniconnect {
namespace auth {

using namespace utils;
using namespace utils::crypto;

std::string JwtService::signPayload(const std::string& payload_json) const {
    const std::string head = base64UrlEncode("{\"alg\":\"HS256\",\"typ\":\"JWT\"}");
    const std::string body = base64UrlEncode(payload_json);
    const std::string signing_input = head + "." + body;
    return signing_input + "." + base64UrlEncode(hmacSha256(secret_, signing_input));
}

std::string JwtService::issue(int user_id, const std::string& role, long long ttl_seconds) const {
    const long long now = static_cast<long long>(std::time(nullptr));
    auto p = JsonValue::object();
    p["sub"] = std::to_string(user_id);
    p["role"] = role;
    p["iat"] = now;
    p["exp"] = now + ttl_seconds;
    return signPayload(p.serialize(false));
}

bool JwtService::verify(const std::string& token, JwtClaims& out) const {
    const size_t d1 = token.find('.');
    if (d1 == std::string::npos) return false;
    const size_t d2 = token.find('.', d1 + 1);
    if (d2 == std::string::npos || token.find('.', d2 + 1) != std::string::npos) return false;

    std::string headRaw, sigRaw, bodyRaw;
    if (!base64UrlDecode(token.substr(0, d1), headRaw)) return false;
    if (!base64UrlDecode(token.substr(d2 + 1), sigRaw)) return false;

    JsonValue head;
    if (!JsonValue::tryParse(headRaw, head) || !head.isObject()) return false;
    if (head["alg"].asString() != "HS256") return false;      // rejects "none" and every other algorithm

    const std::string expected = hmacSha256(secret_, token.substr(0, d2));
    if (!constantTimeEquals(sigRaw, expected)) return false;

    if (!base64UrlDecode(token.substr(d1 + 1, d2 - d1 - 1), bodyRaw)) return false;
    JsonValue body;
    if (!JsonValue::tryParse(bodyRaw, body) || !body.isObject()) return false;
    if (!body.contains("exp") || !body.contains("sub")) return false;

    const long long exp = static_cast<long long>(body["exp"].asDouble(0));
    if (exp <= static_cast<long long>(std::time(nullptr))) return false;

    JwtClaims c;
    c.user_id = body["sub"].asInt(0);
    if (c.user_id <= 0) return false;
    c.role = body["role"].asString();
    c.issued_at = static_cast<long long>(body["iat"].asDouble(0));
    c.expires_at = exp;
    out = c;
    return true;
}

} // namespace auth
} // namespace uniconnect
