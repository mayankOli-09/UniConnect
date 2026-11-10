#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace uniconnect {
namespace utils {

class Crypto {
public:
    // Base64 encode and decode
    static std::string base64Encode(const unsigned char* data, size_t len);
    static std::string base64Encode(const std::string& input);
    static std::string base64Decode(const std::string& input);

    // SHA-1 (required for RFC 6455 WebSocket Handshake)
    static std::vector<uint8_t> sha1(const std::string& input);
    static std::string sha1Hex(const std::string& input);

    // SHA-256 (for hashing passwords and tokens)
    static std::vector<uint8_t> sha256(const std::string& input);
    static std::string sha256Hex(const std::string& input);

    // HMAC-SHA256 (for JWT signature)
    static std::vector<uint8_t> hmacSha256(const std::string& key, const std::string& data);
    static std::string hmacSha256Hex(const std::string& key, const std::string& data);

    // Password hashing with salt
    static std::string hashPassword(const std::string& password, const std::string& salt = "uniconnect_salt_2026");
    static bool verifyPassword(const std::string& password, const std::string& hash, const std::string& salt = "uniconnect_salt_2026");

    // WebSocket Handshake response key generator
    static std::string generateWebSocketAcceptKey(const std::string& clientKey);

    // Simple JWT-like token generator / verifier
    static std::string createJWT(const std::string& payloadJson, const std::string& secret);
    static bool verifyJWT(const std::string& token, const std::string& secret, std::string& outPayload);
};

} // namespace utils
} // namespace uniconnect
