/*
 *  ShieldWall DDoS Protection Suite
 *  Author: fa33az
 */

#define FILTERSCRIPT
#include <a_samp>
#include <shieldwall>

public OnFilterScriptInit()
{
    print("\n--------------------------------------");
    print(" ShieldWall Administration Loaded");
    print("--------------------------------------\n");
    SW_SetRateLimit(10);
    return 1;
}

public OnFilterScriptExit()
{
    print("ShieldWall Administration Unloaded.");
    return 1;
}

public OnPlayerCommandText(playerid, cmdtext[])
{
    if (strcmp(cmdtext, "/shield", true, 7) == 0)
    {
        if (!IsPlayerAdmin(playerid))
        {
            SendClientMessage(playerid, 0xFF0000FF, "Error: You are not authorized to use this command.");
            return 1;
        }

        new cmd[32], args[128];
        new idx = 7;
        while (cmdtext[idx] == ' ') idx++;
        
        new len = 0;
        while (cmdtext[idx] != '\0' && cmdtext[idx] != ' ' && len < 31)
        {
            cmd[len++] = cmdtext[idx++];
        }
        cmd[len] = '\0';

        while (cmdtext[idx] == ' ') idx++;
        format(args, sizeof(args), "%s", cmdtext[idx]);

        if (strcmp(cmd, "status", true) == 0)
        {
            SendClientMessage(playerid, 0x00FF00FF, "[ShieldWall] ShieldWall protection is active.");
            SendClientMessage(playerid, 0xFFFFFFFF, "Layer 1: C++ packet filter is active.");
            SendClientMessage(playerid, 0xFFFFFFFF, "Layer 2: Python query proxy cache is active.");
            return 1;
        }
        else if (strcmp(cmd, "ban", true) == 0)
        {
            new ip[32], duration_str[16], reason[64];
            new sidx = 0;
            while (args[sidx] != '\0' && args[sidx] != ' ' && sidx < 31)
            {
                ip[sidx] = args[sidx];
                sidx++;
            }
            ip[sidx] = '\0';
            
            while (args[sidx] == ' ') sidx++;
            new didx = 0;
            while (args[sidx] != '\0' && args[sidx] != ' ' && didx < 15)
            {
                duration_str[didx++] = args[sidx++];
            }
            duration_str[didx] = '\0';

            while (args[sidx] == ' ') sidx++;
            format(reason, sizeof(reason), "%s", args[sidx]);

            if (ip[0] == '\0' || duration_str[0] == '\0')
            {
                SendClientMessage(playerid, 0xFF0000FF, "Usage: /shield ban <IP> <duration_sec> [reason]");
                return 1;
            }

            new duration = strval(duration_str);
            if (reason[0] == '\0') format(reason, sizeof(reason), "Admin Ban");

            if (SW_BanIP(ip, duration, reason))
            {
                new msg[128];
                format(msg, sizeof(msg), "[ShieldWall] Successfully banned IP %s for %d seconds. Reason: %s", ip, duration, reason);
                SendClientMessage(playerid, 0x00FF00FF, msg);
            }
            else
            {
                SendClientMessage(playerid, 0xFF0000FF, "[ShieldWall] Failed to ban IP.");
            }
            return 1;
        }
        else if (strcmp(cmd, "unban", true) == 0)
        {
            if (args[0] == '\0')
            {
                SendClientMessage(playerid, 0xFF0000FF, "Usage: /shield unban <IP>");
                return 1;
            }

            if (SW_UnbanIP(args))
            {
                new msg[128];
                format(msg, sizeof(msg), "[ShieldWall] Successfully unbanned IP %s.", args);
                SendClientMessage(playerid, 0x00FF00FF, msg);
            }
            else
            {
                SendClientMessage(playerid, 0xFF0000FF, "[ShieldWall] Failed to unban IP. It might not be banned.");
            }
            return 1;
        }
        else if (strcmp(cmd, "check", true) == 0)
        {
            if (args[0] == '\0')
            {
                SendClientMessage(playerid, 0xFF0000FF, "Usage: /shield check <IP>");
                return 1;
            }

            new msg[128];
            if (SW_IsIPBanned(args))
            {
                format(msg, sizeof(msg), "[ShieldWall] IP %s is currently BANNED.", args);
                SendClientMessage(playerid, 0xFF0000FF, msg);
            }
            else
            {
                format(msg, sizeof(msg), "[ShieldWall] IP %s is NOT banned.", args);
                SendClientMessage(playerid, 0x00FF00FF, msg);
            }
            return 1;
        }
        else if (strcmp(cmd, "limit", true) == 0)
        {
            if (args[0] == '\0')
            {
                SendClientMessage(playerid, 0xFF0000FF, "Usage: /shield limit <packets_per_sec>");
                return 1;
            }
            
            new limit = strval(args);
            SW_SetRateLimit(limit);
            new msg[128];
            format(msg, sizeof(msg), "[ShieldWall] Set rate limit to %d packets/sec.", limit);
            SendClientMessage(playerid, 0x00FF00FF, msg);
            return 1;
        }
        
        SendClientMessage(playerid, 0xFFFFFF, "ShieldWall Commands: status, ban, unban, check, limit");
        return 1;
    }
    return 0;
}
