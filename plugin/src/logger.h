/*
 *  ShieldWall DDoS Protection Suite
 *  Author: fa33az
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>
#include <fstream>

class Logger {
public:
    static Logger& Instance();
    void Initialize(const std::string& filepath);
    void Log(const std::string& level, const char* format, ...);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream file_;
    std::mutex mutex_;
    bool initialized_ = false;
};

#define LOG_INFO(format, ...) Logger::Instance().Log("INFO", format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Logger::Instance().Log("WARN", format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Logger::Instance().Log("ERROR", format, ##__VA_ARGS__)

#endif // LOGGER_H
