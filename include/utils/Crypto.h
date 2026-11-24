#pragma once
// Hashing and encoding primitives (SHA-256, HMAC, PBKDF2, hex, base64url, secure random).
// Byte strings are carried in std::string.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace uniconnect {
namespace utils {
namespace crypto {

std::string sha1(const std::string& data);                                    // 20 raw bytes (WebSocket handshake only)
std::string sha256(const std::string& data);                                  // 32 raw bytes
std::string hmacSha256(const std::string& key, const std::string& data);      // 32 raw bytes
std::string pbkdf2Sha256(const std::string& password, const std::string& salt,
                         unsigned iterations, size_t dkLen = 32);             // raw bytes

std::string toHex(const std::string& bytes);
bool fromHex(const std::string& hex, std::string& out);

std::string base64Encode(const std::string& bytes);                           // standard alphabet, with padding
std::string base64UrlEncode(const std::string& bytes);                        // no padding
bool base64UrlDecode(const std::string& text, std::string& out);

bool constantTimeEquals(const std::string& a, const std::string& b);

// Cryptographically secure random bytes from the Windows CNG (throws std::runtime_error on failure).
std::string randomBytes(size_t n);

} // namespace crypto
} // namespace utils
} // namespace uniconnect
