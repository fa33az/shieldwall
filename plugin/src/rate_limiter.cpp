/*
 *  ShieldWall DDoS Protection Suite
 *  Author: fa33az
 */

#include "rate_limiter.h"
#include <chrono>

RateLimiter& RateLimiter::Instance() {
    static RateLimiter instance;
    return instance;
}

void RateLimiter::SetLimit(int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    limit_ = limit;
}

void RateLimiter::SetWindowSize(double seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    window_seconds_ = seconds;
}

bool RateLimiter::CheckLimit(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now_epoch = std::chrono::steady_clock::now().time_since_epoch();
    double now = std::chrono::duration<double>(now_epoch).count();

    auto& timestamps = tracker_[ip];

    std::vector<double> filtered;
    for (double ts : timestamps) {
        if (now - ts <= window_seconds_) {
            filtered.push_back(ts);
        }
    }

    filtered.push_back(now);
    timestamps = std::move(filtered);

    if (static_cast<int>(timestamps.size()) > limit_) {
        return false;
    }

    return true;
}

void RateLimiter::Clear(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    tracker_.erase(ip);
}

void RateLimiter::ClearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    tracker_.clear();
}
