/*
 *  ShieldWall DDoS Protection Suite
 *  Author: fa33az
 */

#include "database.h"
#include "logger.h"
#include <ctime>
#include <iostream>
#include <filesystem>

Database& Database::Instance() {
    static Database instance;
    return instance;
}

Database::~Database() {
    Close();
}

bool Database::Initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;

    try {
        std::filesystem::path path(dbPath);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create database directory: %s", e.what());
    }

    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Cannot open database: %s", sqlite3_errmsg(db_));
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    const char* sql = 
        "CREATE TABLE IF NOT EXISTS bans ("
        "ip TEXT PRIMARY KEY, "
        "banned_at INTEGER, "
        "expires_at INTEGER, "
        "reason TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS whitelist ("
        "ip TEXT PRIMARY KEY"
        ");";

    char* err_msg = nullptr;
    rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        LOG_ERROR("SQL error creating tables: %s", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    initialized_ = true;
    LOG_INFO("Database initialized successfully at: %s", dbPath.c_str());
    return true;
}

void Database::Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
        initialized_ = false;
        LOG_INFO("Database connection closed.");
    }
}

bool Database::AddBan(const std::string& ip, int durationSecs, const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !db_) return false;

    long long now = std::time(nullptr);
    long long expires = now + durationSecs;

    const char* sql = "INSERT OR REPLACE INTO bans (ip, banned_at, expires_at, reason) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare AddBan statement: %s", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_int64(stmt, 3, expires);
    sqlite3_bind_text(stmt, 4, reason.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute AddBan statement: %s", sqlite3_errmsg(db_));
        return false;
    }

    return true;
}

bool Database::RemoveBan(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !db_) return false;

    const char* sql = "DELETE FROM bans WHERE ip = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare RemoveBan statement: %s", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute RemoveBan statement: %s", sqlite3_errmsg(db_));
        return false;
    }

    return true;
}

bool Database::IsBanned(const std::string& ip, std::string& outReason) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !db_) return false;

    long long now = std::time(nullptr);
    const char* sql = "SELECT reason, expires_at FROM bans WHERE ip = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare IsBanned statement: %s", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);

    bool isBanned = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        long long expires = sqlite3_column_int64(stmt, 1);
        if (expires > now) {
            const unsigned char* reason_text = sqlite3_column_text(stmt, 0);
            outReason = reason_text ? reinterpret_cast<const char*>(reason_text) : "";
            isBanned = true;
        }
    }

    sqlite3_finalize(stmt);
    return isBanned;
}

std::vector<BanEntry> Database::GetActiveBans() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<BanEntry> list;
    if (!initialized_ || !db_) return list;

    long long now = std::time(nullptr);
    const char* sql = "SELECT ip, banned_at, expires_at, reason FROM bans WHERE expires_at > ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare GetActiveBans statement: %s", sqlite3_errmsg(db_));
        return list;
    }

    sqlite3_bind_int64(stmt, 1, now);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        BanEntry entry;
        const unsigned char* ip_text = sqlite3_column_text(stmt, 0);
        entry.ip = ip_text ? reinterpret_cast<const char*>(ip_text) : "";
        entry.banned_at = sqlite3_column_int64(stmt, 1);
        entry.expires_at = sqlite3_column_int64(stmt, 2);
        const unsigned char* reason_text = sqlite3_column_text(stmt, 3);
        entry.reason = reason_text ? reinterpret_cast<const char*>(reason_text) : "";
        list.push_back(entry);
    }

    sqlite3_finalize(stmt);
    return list;
}

void Database::CleanExpiredBans() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !db_) return;

    long long now = std::time(nullptr);
    const char* sql = "DELETE FROM bans WHERE expires_at <= ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare CleanExpiredBans statement: %s", sqlite3_errmsg(db_));
        return;
    }

    sqlite3_bind_int64(stmt, 1, now);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute CleanExpiredBans statement: %s", sqlite3_errmsg(db_));
    }
}

bool Database::AddWhitelist(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !db_) return false;

    const char* sql = "INSERT OR REPLACE INTO whitelist (ip) VALUES (?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare AddWhitelist statement: %s", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute AddWhitelist statement: %s", sqlite3_errmsg(db_));
        return false;
    }

    return true;
}

bool Database::RemoveWhitelist(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !db_) return false;

    const char* sql = "DELETE FROM whitelist WHERE ip = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare RemoveWhitelist statement: %s", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute RemoveWhitelist statement: %s", sqlite3_errmsg(db_));
        return false;
    }

    return true;
}

bool Database::IsWhitelisted(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !db_) return false;

    const char* sql = "SELECT 1 FROM whitelist WHERE ip = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare IsWhitelisted statement: %s", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);

    bool isWhitelisted = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        isWhitelisted = true;
    }

    sqlite3_finalize(stmt);
    return isWhitelisted;
}
