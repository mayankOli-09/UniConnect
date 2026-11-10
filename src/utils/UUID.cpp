#include "utils/UUID.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace uniconnect {
namespace utils {

std::string UUID::generate() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;

    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);

    // Set UUID version 4 (bits 12-15 of time_hi_and_version to 0100)
    part1 = (part1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    // Set variant (bits 6-7 of clock_seq_hi_and_reserved to 10)
    part2 = (part2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << (part1 >> 32) << "-"
        << std::setw(4) << ((part1 >> 16) & 0xFFFF) << "-"
        << std::setw(4) << (part1 & 0xFFFF) << "-"
        << std::setw(4) << (part2 >> 48) << "-"
        << std::setw(12) << (part2 & 0xFFFFFFFFFFFFULL);

    return oss.str();
}

} // namespace utils
} // namespace uniconnect
