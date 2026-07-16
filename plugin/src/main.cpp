/*
 *  ShieldWall DDoS Protection Suite
 *  Author: fa33az
 */

#include <string>
#include <vector>
#include <mutex>
#include "plugincommon.h"
#include "amx/amx.h"
#include "logger.h"
#include "database.h"
#include "rate_limiter.h"
#include "raknet_hook.h"

typedef void (*logprintf_t)(const char* format, ...);
logprintf_t logprintf;

// Native: SW_BanIP(const ip[], duration_secs, const reason[]);
static cell AMX_NATIVE_CALL n_SW_BanIP(AMX* amx, cell* params) {
    char* ip = nullptr;
    amx_StrParam(amx, params[1], ip);

    int duration = static_cast<int>(params[2]);

    char* reason = nullptr;
    amx_StrParam(amx, params[3], reason);

    if (!ip) return 0;
    return Database::Instance().AddBan(ip, duration, reason ? reason : "None") ? 1 : 0;
}

// Native: SW_UnbanIP(const ip[]);
static cell AMX_NATIVE_CALL n_SW_UnbanIP(AMX* amx, cell* params) {
    char* ip = nullptr;
    amx_StrParam(amx, params[1], ip);

    if (!ip) return 0;
    return Database::Instance().RemoveBan(ip) ? 1 : 0;
}

// Native: SW_IsIPBanned(const ip[]);
static cell AMX_NATIVE_CALL n_SW_IsIPBanned(AMX* amx, cell* params) {
    char* ip = nullptr;
    amx_StrParam(amx, params[1], ip);

    if (!ip) return 0;
    std::string reason;
    return Database::Instance().IsBanned(ip, reason) ? 1 : 0;
}

// Native: SW_ClearBans();
static cell AMX_NATIVE_CALL n_SW_ClearBans(AMX* amx, cell* params) {
    RateLimiter::Instance().ClearAll();
    return 1;
}

// Native: SW_SetRateLimit(packets_per_sec);
static cell AMX_NATIVE_CALL n_SW_SetRateLimit(AMX* amx, cell* params) {
    int limit = static_cast<int>(params[1]);
    RateLimiter::Instance().SetLimit(limit);
    return 1;
}

const AMX_NATIVE_INFO SW_Natives[] = {
    { "SW_BanIP", n_SW_BanIP },
    { "SW_UnbanIP", n_SW_UnbanIP },
    { "SW_IsIPBanned", n_SW_IsIPBanned },
    { "SW_ClearBans", n_SW_ClearBans },
    { "SW_SetRateLimit", n_SW_SetRateLimit },
    { nullptr, nullptr }
};

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() {
    return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void** ppData) {
    logprintf = (logprintf_t)ppData[PLUGIN_DATA_LOGPRINTF];
    
    Logger::Instance().Initialize("logs/shieldwall.log");
    LOG_INFO("ShieldWall plugin is loading...");

    if (!Database::Instance().Initialize("scriptfiles/shieldwall.db")) {
        LOG_ERROR("Failed to initialize database. Plugin loading aborted.");
        return false;
    }

    typedef void* (*PluginGetRakServer_t)();
    PluginGetRakServer_t PluginGetRakServer = (PluginGetRakServer_t)ppData[PLUGIN_DATA_RAKSERVER];
    if (PluginGetRakServer) {
        void* rakPeer = PluginGetRakServer();
        if (rakPeer) {
            InstallHooks(rakPeer);
        } else {
            LOG_ERROR("RakServer pointer is NULL!");
        }
    } else {
        LOG_ERROR("PluginGetRakServer export not found!");
    }

    LOG_INFO("ShieldWall loaded successfully!");
    return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload() {
    LOG_INFO("ShieldWall is unloading...");
    UninstallHooks();
    Database::Instance().Close();
    LOG_INFO("ShieldWall unloaded.");
}

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX* amx) {
    return amx_Register(amx, SW_Natives, -1);
}

PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX* amx) {
    return AMX_ERR_NONE;
}
