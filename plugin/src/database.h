/*
 *  ShieldWall DDoS Protection Suite
 *  Author: fa33az
 */

#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <mutex>
#include "sqlite3.h"

struct BanEntry {
    std::string ip;
    long long banned_at;
    long long expires_at;
    std::string reason;
};

class Database {
public:
    static Database& Instance();
    bool Initialize(const std::string& dbPath);
    void Close();

    bool AddBan(const std::string& ip, int durationSecs, const std::string& reason);
    bool RemoveBan(const std::string& ip);
    bool IsBanned(const std::string& ip, std::string& outReason);
    std::vector<BanEntry> GetActiveBans();
    void CleanExpiredBans();

    // Whitelist management
    bool AddWhitelist(const std::string& ip);
    bool RemoveWhitelist(const std::string& ip);
    bool IsWhitelisted(const std::string& ip);

private:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* db_ = nullptr;
    std::mutex mutex_;
    bool initialized_ = false;
};

#endif // DATABASE_H
