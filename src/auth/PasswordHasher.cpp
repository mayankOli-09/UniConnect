#include "auth/PasswordHasher.h"

#include <cstdlib>

#include "utils/Crypto.h"

namespace uniconnect {
namespace auth {

using namespace utils::crypto;

std::string PasswordHasher::hash(const std::string& password, unsigned iterations) {
    const std::string salt = randomBytes(16);
    const std::string dk = pbkdf2Sha256(password, salt, iterations, 32);
    return "pbkdf2_sha256$" + std::to_string(iterations) + "$" + toHex(salt) + "$" + toHex(dk);
}

bool PasswordHasher::verify(const std::string& password, const std::string& stored) {
    const std::string prefix = "pbkdf2_sha256$";
    if (stored.compare(0, prefix.size(), prefix) != 0) return false;
    const size_t p1 = stored.find('$', prefix.size());
    if (p1 == std::string::npos) return false;
    const size_t p2 = stored.find('$', p1 + 1);
    if (p2 == std::string::npos) return false;

    const std::string iterText = stored.substr(prefix.size(), p1 - prefix.size());
    if (iterText.empty() || iterText.size() > 8) return false;
    for (char c : iterText) if (c < '0' || c > '9') return false;
    const unsigned long iterations = std::strtoul(iterText.c_str(), nullptr, 10);
    if (iterations < 1 || iterations > 2000000) return false;   // refuse absurd work factors

    std::string salt, expected;
    if (!fromHex(stored.substr(p1 + 1, p2 - p1 - 1), salt) || salt.empty()) return false;
    if (!fromHex(stored.substr(p2 + 1), expected) || expected.empty()) return false;

    const std::string actual = pbkdf2Sha256(password, salt, static_cast<unsigned>(iterations), expected.size());
    return constantTimeEquals(actual, expected);
}

} // namespace auth
} // namespace uniconnect
