#pragma once

#include <string>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iostream>

namespace uniconnect {
namespace utils {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static Logger& getInstance();

    void setLevel(LogLevel level);
    LogLevel getLevel() const;

    void log(LogLevel level, const std::string& module, const std::string& message);

    template<typename... Args>
    void debug(const std::string& module, Args&&... args) {
        log(LogLevel::DEBUG, module, format(std::forward<Args>(args)...));
    }

    template<typename... Args>
    void info(const std::string& module, Args&&... args) {
        log(LogLevel::INFO, module, format(std::forward<Args>(args)...));
    }

    template<typename... Args>
    void warn(const std::string& module, Args&&... args) {
        log(LogLevel::WARN, module, format(std::forward<Args>(args)...));
    }

    template<typename... Args>
    void error(const std::string& module, Args&&... args) {
        log(LogLevel::ERROR, module, format(std::forward<Args>(args)...));
    }

private:
    Logger();
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    template<typename T>
    void formatHelper(std::ostringstream& oss, T&& val) {
        oss << std::forward<T>(val);
    }

    template<typename T, typename... Rest>
    void formatHelper(std::ostringstream& oss, T&& first, Rest&&... rest) {
        oss << std::forward<T>(first) << " ";
        formatHelper(oss, std::forward<Rest>(rest)...);
    }

    template<typename... Args>
    std::string format(Args&&... args) {
        std::ostringstream oss;
        formatHelper(oss, std::forward<Args>(args)...);
        return oss.str();
    }

    std::string getTimestamp();
    const char* levelToString(LogLevel level);
    const char* levelColor(LogLevel level);

    LogLevel current_level_{LogLevel::DEBUG};
    std::mutex log_mutex_;
};

} // namespace utils
} // namespace uniconnect
