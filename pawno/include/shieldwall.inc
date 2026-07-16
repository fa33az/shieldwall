/*
 *  ShieldWall DDoS Protection Suite
 *  Author: fa33az
 */

#if defined _shieldwall_included
	#endinput
#endif
#define _shieldwall_included

/**
 * <summary>Bans an IP address dynamically for a specified duration.</summary>
 * <param name="ip">The IP address string to ban.</param>
 * <param name="duration_secs">The duration of the ban in seconds.</param>
 * <param name="reason">The reason for the ban.</param>
 * <returns>1 on success, 0 on failure.</returns>
 */
native SW_BanIP(const ip[], duration_secs, const reason[]);

/**
 * <summary>Unbans an IP address.</summary>
 * <param name="ip">The IP address string to unban.</param>
 * <returns>1 on success, 0 on failure.</returns>
 */
native SW_UnbanIP(const ip[]);

/**
 * <summary>Checks if an IP address is currently banned.</summary>
 * <param name="ip">The IP address string to check.</param>
 * <returns>1 if banned, 0 otherwise.</returns>
 */
native SW_IsIPBanned(const ip[]);

/**
 * <summary>Clears the internal rate-limiter tracker.</summary>
 * <returns>1 always.</returns>
 */
native SW_ClearBans();

/**
 * <summary>Sets the connection packet rate limit threshold.</summary>
 * <param name="packets_per_sec">The maximum allowed packets per second per IP.</param>
 * <returns>1 always.</returns>
 */
native SW_SetRateLimit(packets_per_sec);

/**
 * <summary>Adds an IP address to the whitelist. Whitelisted IPs bypass rate limits and bans.</summary>
 * <param name="ip">The IP address string to whitelist.</param>
 * <returns>1 on success, 0 on failure.</returns>
 */
native SW_AddWhitelist(const ip[]);

/**
 * <summary>Removes an IP address from the whitelist.</summary>
 * <param name="ip">The IP address string to remove from whitelist.</param>
 * <returns>1 on success, 0 on failure.</returns>
 */
native SW_RemoveWhitelist(const ip[]);

/**
 * <summary>Checks if an IP address is whitelisted.</summary>
 * <param name="ip">The IP address string to check.</param>
 * <returns>1 if whitelisted, 0 otherwise.</returns>
 */
native SW_IsWhitelisted(const ip[]);
