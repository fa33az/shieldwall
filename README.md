# ShieldWall DDoS Protection Suite

ShieldWall is a multi-layer anti-DDoS protection suite designed for SA-MP (San Andreas Multiplayer) and open.mp servers. It consists of three integrated layers: an asynchronous Python query proxy, a C++ RakNet packet filter plugin, and a Pawn administration filterscript.

---

## System Architecture

The protection suite operates on three distinct layers to intercept, filter, and cache network traffic before it can exhaust server resources.

### Layer 1: C++ RakNet Packet Filter Plugin
- Hooks the `RakServerInterface::Receive` method via virtual method table detouring (vtable index 9 on Windows, 10 on Linux).
- Intercepts raw network packets directly in memory.
- Performs sliding-window rate limiting per IP address within a 1.0-second window.
- Drops packets from banned IP addresses instantly, bypassing the main server logic and saving CPU cycles.
- Integrates with an SQLite database for persistent ban synchronization.

### Layer 2: Python Query Proxy
- Listens on the public server port (e.g. 7777) and forwards game traffic transparently to the backend server.
- Intercepts and parses SA-MP query packets.
- Caches query responses (information, rules, clients, and detailed client queries) with a configurable Time-To-Live (default: 2.0 seconds) to prevent query reflection floods.
- Automatically rate-limits query floods and blocks offending IPs for a customizable duration.

### Layer 3: Pawn Administration Filterscript
- Provides in-game commands for server administrators to check protection status, set rate limits, ban IPs, and unban IPs in real-time.
- Interfaces with the SQLite database to sync bans with the C++ filtering layer instantly.

---

## Installation and Setup

### Prerequisites
- Python 3.8 or higher
- C++ Compiler / Build Tools (MSVC for Windows, GCC for Linux)
- SA-MP or open.mp server files

### Step 1: Install the C++ Plugin
1. Place the compiled `shieldwall.dll` (Windows) or `shieldwall.so` (Linux) inside the `plugins/` directory of your SA-MP server.
2. Add the plugin to your `server.cfg` file:
   ```text
   plugins shieldwall
   ```

### Step 2: Set Up the Pawn Include and Filterscript
1. Copy `shieldwall.inc` to your server's `pawno/include/` directory.
2. Compile `shieldwall.pwn` into `shieldwall.amx` and place it in the `filterscripts/` directory.
3. Add the filterscript to your `server.cfg` file:
   ```text
   filterscripts shieldwall
   ```

### Step 3: Configure and Launch the Python Query Proxy
1. Configure your SA-MP server to bind to a backend port (e.g. 7778) in `server.cfg`:
   ```text
   port 7778
   ```
2. Configure the query proxy in `proxy/config.json`:
   ```json
   {
     "proxy_ip": "0.0.0.0",
     "proxy_port": 7777,
     "backend_ip": "127.0.0.1",
     "backend_port": 7778,
     "query_rate_limit": 50,
     "query_ban_duration": 60,
     "cache_ttl": 2.0
   }
   ```
3. Place `start.bat` in the root folder of your SA-MP server and double-click it. This will launch both the Python Query Proxy and the SA-MP server simultaneously.

---

## Administration Commands

The following commands are available to administrators in-game:

| Command | Arguments | Description |
|---|---|---|
| /shield status | None | Displays the current protection status of Layer 1 and Layer 2. |
| /shield ban | [IP] [duration_sec] [reason] | Bans an IP address dynamically for a specified duration. |
| /shield unban | [IP] | Unbans a previously banned IP address. |
| /shield check | [IP] | Checks whether an IP address is currently banned. |
| /shield limit | [packets_per_sec] | Adjusts the packet rate limit threshold dynamically. |

---

## Authors
- fa33az
