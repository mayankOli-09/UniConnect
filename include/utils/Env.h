#pragma once
#include <cstdlib>
#include <string>

namespace uniconnect {
namespace utils {

// Reads an environment variable; returns `def` when it is not set.
inline std::string getEnvVar(const char* name, const std::string& def = "") {
#if defined(_MSC_VER)
    char* v = nullptr;
    size_t len = 0;
    if (_dupenv_s(&v, &len, name) == 0 && v) {
        std::string s(v);
        std::free(v);
        return s;
    }
    return def;
#else
    const char* v = std::getenv(name);
    return v ? std::string(v) : def;
#endif
}

} // namespace utils
} // namespace uniconnect
