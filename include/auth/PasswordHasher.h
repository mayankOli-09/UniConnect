#pragma once
#include <string>

namespace uniconnect {
namespace auth {

// Stored format: pbkdf2_sha256$<iterations>$<salt hex>$<hash hex>
class PasswordHasher {
public:
    static constexpr unsigned DEFAULT_ITERATIONS = 100000;
    static std::string hash(const std::string& password, unsigned iterations = DEFAULT_ITERATIONS);
    // False for a wrong password AND for any stored value that is not in the format above
    // (for example the old placeholder values such as "hash123").
    static bool verify(const std::string& password, const std::string& stored);
};

} // namespace auth
} // namespace uniconnect
