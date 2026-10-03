#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <mutex>
#include <iostream>

enum class LogLevel {
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    // Thread-safe logging methods
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);

    // Generic log method with level
    static void log(LogLevel level, const std::string& message);

private:
    static std::mutex logMutex;
    static std::string levelToString(LogLevel level);
    static std::string getCurrentTimestamp();
};

#endif // LOGGER_HPP
