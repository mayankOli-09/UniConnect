#pragma once
#if !defined(_WIN32)
#error "UniConnect is a Windows-only project."
#endif
// Thread-safe logger (Windows). Usage: Logger::getInstance().info("Tag", "message", value, ...)
// Set environment variable UNICONNECT_LOG=debug to also see debug lines.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace uniconnect {
namespace utils {

class Logger {
public:
    static Logger& getInstance() {
        static Logger inst;
        return inst;
    }
    template <typename... A> void debug(const std::string& tag, const A&... a) { if (debug_) log("DEBUG", tag, a...); }
    template <typename... A> void info(const std::string& tag, const A&... a)  { log("INFO ", tag, a...); }
    template <typename... A> void warn(const std::string& tag, const A&... a)  { log("WARN ", tag, a...); }
    template <typename... A> void error(const std::string& tag, const A&... a) { log("ERROR", tag, a...); }

private:
    Logger() {
#if defined(_MSC_VER)
        char* lvl = nullptr;
        size_t len = 0;
        if (_dupenv_s(&lvl, &len, "UNICONNECT_LOG") == 0 && lvl) {
            debug_ = std::string(lvl) == "debug";
            std::free(lvl);
        }
#else
        const char* lvl = std::getenv("UNICONNECT_LOG");
        debug_ = lvl && std::string(lvl) == "debug";
#endif
    }
    template <typename... A> void log(const char* level, const std::string& tag, const A&... a) {
        std::ostringstream line;
        line << '[' << timestamp() << "] [" << level << "] [" << tag << ']';
        ((line << ' ' << a), ...);
        line << '\n';
        std::lock_guard<std::mutex> lock(m_);
        std::cout << line.str();
        std::cout.flush();
    }
    static std::string timestamp() {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &t);
        char buf[24];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        return buf;
    }
    std::mutex m_;
    bool debug_ = false;
};

} // namespace utils
} // namespace uniconnect
