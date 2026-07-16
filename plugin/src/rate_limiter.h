/*
 *  ShieldWall DDoS Protection Suite
 *  Author: fa33az
 */

#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

class RateLimiter {
public:
    static RateLimiter& Instance();

    void SetLimit(int limit);
    void SetWindowSize(double seconds);
    bool CheckLimit(const std::string& ip);
    void Clear(const std::string& ip);
    void ClearAll();

private:
    RateLimiter() = default;
    ~RateLimiter() = default;
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;

    std::unordered_map<std::string, std::vector<double>> tracker_;
    std::mutex mutex_;
    int limit_ = 10;
    double window_seconds_ = 1.0;
};

#endif // RATE_LIMITER_H
