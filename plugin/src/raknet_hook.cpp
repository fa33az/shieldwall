/*
 *  ShieldWall DDoS Protection Suite
 *  Author: fa33az
 */

#include "raknet_hook.h"
#include "logger.h"
#include "database.h"
#include "rate_limiter.h"
#include "raknet/RakServerInterface.h"
#include <vector>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#define RAKNET_CALL __fastcall
#define RAKNET_ARGS(p) void* p, void* edx
#define RAKNET_PASS(p) p, nullptr
#else
#include <sys/mman.h>
#include <unistd.h>
#define RAKNET_CALL
#define RAKNET_ARGS(p) void* p
#define RAKNET_PASS(p) p
#endif

typedef Packet* (RAKNET_CALL* Receive_t)(RAKNET_ARGS(pRakServer));
Receive_t pOriginalReceive = nullptr;

#ifdef _WIN32
const int RECEIVE_INDEX = 9;
#else
const int RECEIVE_INDEX = 10;
#endif

void** pVmt = nullptr;
void* originalVmt = nullptr;

// Implement PlayerID::ToString since it is declared in headers but not linked
char* PlayerID::ToString(bool writePort) const {
    static char dest[64];
    const unsigned char* ipBytes = reinterpret_cast<const unsigned char*>(&binaryAddress);
    
    char ip_str[32];
    sprintf(ip_str, "%d.%d.%d.%d", ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);

    if (writePort) {
        sprintf(dest, "%s:%d", ip_str, port);
    } else {
        sprintf(dest, "%s", ip_str);
    }
    return dest;
}

Packet* RAKNET_CALL Hooked_Receive(RAKNET_ARGS(pRakServer)) {
    RakServerInterface* pServer = static_cast<RakServerInterface*>(pRakServer);
    Packet* packet = nullptr;

    while (true) {
        packet = pOriginalReceive(RAKNET_PASS(pRakServer));
        if (!packet) {
            break;
        }

        std::string ip = packet->playerId.ToString(false);

        std::string banReason;
        if (Database::Instance().IsBanned(ip, banReason)) {
            LOG_INFO("Dropped packet from banned IP: %s (Reason: %s)", ip.c_str(), banReason.c_str());
            pServer->DeallocatePacket(packet);
            continue;
        }

        if (!RateLimiter::Instance().CheckLimit(ip)) {
            LOG_WARN("IP %s exceeded packet rate limit. Banning for 5 minutes.", ip.c_str());
            Database::Instance().AddBan(ip, 300, "Packet rate limit exceeded");
            pServer->DeallocatePacket(packet);
            continue;
        }

        break;
    }

    return packet;
}

void InstallHooks(void* rakPeer) {
    if (!rakPeer) return;

    void*** pObject = reinterpret_cast<void***>(rakPeer);
    pVmt = *pObject;
    originalVmt = pVmt;

    pOriginalReceive = reinterpret_cast<Receive_t>(pVmt[RECEIVE_INDEX]);

#ifdef _WIN32
    DWORD oldProtect;
    VirtualProtect(&pVmt[RECEIVE_INDEX], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
    pVmt[RECEIVE_INDEX] = reinterpret_cast<void*>(&Hooked_Receive);
    VirtualProtect(&pVmt[RECEIVE_INDEX], sizeof(void*), oldProtect, &oldProtect);
#else
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t addr = reinterpret_cast<uintptr_t>(&pVmt[RECEIVE_INDEX]);
    uintptr_t pageStart = addr & ~(pageSize - 1);
    mprotect(reinterpret_cast<void*>(pageStart), pageSize, PROT_READ | PROT_WRITE);
    pVmt[RECEIVE_INDEX] = reinterpret_cast<void*>(&Hooked_Receive);
    mprotect(reinterpret_cast<void*>(pageStart), pageSize, PROT_READ | PROT_EXEC);
#endif

    LOG_INFO("RakPeer::Receive hook installed successfully!");
}

void UninstallHooks() {
    if (pVmt && pOriginalReceive) {
#ifdef _WIN32
        DWORD oldProtect;
        VirtualProtect(&pVmt[RECEIVE_INDEX], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        pVmt[RECEIVE_INDEX] = reinterpret_cast<void*>(pOriginalReceive);
        VirtualProtect(&pVmt[RECEIVE_INDEX], sizeof(void*), oldProtect, &oldProtect);
#else
        size_t pageSize = sysconf(_SC_PAGESIZE);
        uintptr_t addr = reinterpret_cast<uintptr_t>(&pVmt[RECEIVE_INDEX]);
        uintptr_t pageStart = addr & ~(pageSize - 1);
        mprotect(reinterpret_cast<void*>(pageStart), pageSize, PROT_READ | PROT_WRITE);
        pVmt[RECEIVE_INDEX] = reinterpret_cast<void*>(pOriginalReceive);
        mprotect(reinterpret_cast<void*>(pageStart), pageSize, PROT_READ | PROT_EXEC);
#endif
        LOG_INFO("RakPeer::Receive hook uninstalled.");
    }
}
