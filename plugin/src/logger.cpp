/*
 *  ShieldWall DDoS Protection Suite
 *  Author: fa33az
 */

#include "logger.h"
#include <cstdarg>
#include <ctime>
#include <iostream>
#include <filesystem>

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::Initialize(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;

    try {
        std::filesystem::path path(filepath);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        file_.open(filepath, std::ios::out | std::ios::app);
        if (file_.is_open()) {
            initialized_ = true;
        } else {
            std::cerr << "[ShieldWall Logger] Failed to open log file: " << filepath << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ShieldWall Logger] Exception initializing logger: " << e.what() << std::endl;
    }
}

void Logger::Log(const std::string& level, const char* format, ...) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Get current timestamp
    std::time_t now = std::time(nullptr);
    std::tm time_info;
#ifdef _WIN32
    localtime_s(&time_info, &now);
#else
    localtime_r(&now, &time_info);
#endif

    char time_str[20];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &time_info);

    // Format log message
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Print to stdout/stderr in server console
    std::cout << "[ShieldWall] [" << level << "] " << message << std::endl;

    if (initialized_ && file_.is_open()) {
        file_ << "[" << time_str << "] [" << level << "] " << message << "\n";
        file_.flush();
    }
}
