# ShieldWall DDoS Protection Suite
# Author: fa33az

import asyncio
import socket
import json
import time
import os
import sys

class QueryProxy:
    def __init__(self, config_path):
        with open(config_path, 'r') as f:
            self.config = json.load(f)

        self.proxy_ip = self.config['proxy_ip']
        self.proxy_port = self.config['proxy_port']
        self.backend_ip = self.config['backend_ip']
        self.backend_port = self.config['backend_port']
        self.query_rate_limit = self.config['query_rate_limit']
        self.query_ban_duration = self.config['query_ban_duration']
        self.cache_ttl = self.config['cache_ttl']
        self.whitelist_ips = set(self.config.get('whitelist_ips', []))

        self.cache = {}
        self.rate_limiter = {}
        self.ban_list = {}
        self.clients = {}

        # Stats counters
        self.stats_cache_hits = 0
        self.stats_forwards = 0
        self.stats_blocked = 0

    def is_banned(self, ip):
        now = time.time()
        if ip in self.ban_list:
            if now < self.ban_list[ip]:
                return True
            else:
                del self.ban_list[ip]
        return False

    def check_rate_limit(self, ip):
        if ip in self.whitelist_ips:
            return True

        if self.is_banned(ip):
            return False

        now = time.time()
        timestamps = self.rate_limiter.get(ip, [])
        timestamps = [ts for ts in timestamps if now - ts <= 1.0]
        timestamps.append(now)
        self.rate_limiter[ip] = timestamps

        if len(timestamps) > self.query_rate_limit:
            print(f"[Proxy] IP {ip} rate limited. Banning for {self.query_ban_duration} seconds.")
            self.ban_list[ip] = now + self.query_ban_duration
            asyncio.create_task(add_firewall_rule(ip))
            return False
        return True

async def add_firewall_rule(ip):
    if os.name == 'nt':
        cmd = f'netsh advfirewall firewall add rule name="ShieldWall-Ban-{ip}" dir=in action=block remoteip={ip}'
    else:
        cmd = f'iptables -A INPUT -s {ip} -j DROP'
    try:
        print(f"[Proxy] Adding OS firewall block rule for IP: {ip}")
        proc = await asyncio.create_subprocess_shell(cmd, stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
        await proc.wait()
    except Exception as e:
        print(f"[Proxy] Failed to add firewall rule: {e}")

async def remove_firewall_rule(ip):
    if os.name == 'nt':
        cmd = f'netsh advfirewall firewall delete rule name="ShieldWall-Ban-{ip}"'
    else:
        cmd = f'iptables -D INPUT -s {ip} -j DROP'
    try:
        print(f"[Proxy] Removing OS firewall block rule for IP: {ip}")
        proc = await asyncio.create_subprocess_shell(cmd, stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
        await proc.wait()
    except Exception as e:
        print(f"[Proxy] Failed to remove firewall rule: {e}")

async def clear_existing_firewall_rules():
    print("[Proxy] Cleaning up any leftover ShieldWall firewall rules...")
    if os.name == 'nt':
        cmd = 'powershell -Command "Remove-NetFirewallRule -DisplayName \'ShieldWall-Ban-*\' -ErrorAction SilentlyContinue"'
    else:
        # Silently try to delete input rules (this is safe if none exist)
        cmd = 'iptables -F' # Or just ignore for Linux non-root runs
    try:
        proc = await asyncio.create_subprocess_shell(cmd, stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
        await proc.wait()
    except Exception:
        pass

class BackendProxyProtocol(asyncio.DatagramProtocol):
    def __init__(self, proxy, client_addr):
        self.proxy = proxy
        self.client_addr = client_addr
        self.transport = None
        self.last_active = time.time()

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, addr):
        self.last_active = time.time()
        self.proxy.server_transport.sendto(data, self.client_addr)

    def connection_lost(self, exc):
        pass

class ProxyServerProtocol(asyncio.DatagramProtocol):
    def __init__(self, proxy):
        self.proxy = proxy
        self.transport = None

    def connection_made(self, transport):
        self.transport = transport
        self.proxy.server_transport = transport
        print(f"[Proxy] Listening on UDP {self.proxy.proxy_ip}:{self.proxy.proxy_port}")

    def datagram_received(self, data, addr):
        client_ip, client_port = addr

        if data.startswith(b'SAMP') and len(data) >= 11:
            if not self.proxy.check_rate_limit(client_ip):
                self.proxy.stats_blocked += 1
                return

            query_type = data[10:11]
            
            if query_type == b'p':
                self.proxy.stats_forwards += 1
                asyncio.create_task(self.forward_query(data, addr))
                return

            now = time.time()
            if query_type in self.proxy.cache:
                cached_data, timestamp = self.proxy.cache[query_type]
                if now - timestamp <= self.proxy.cache_ttl:
                    self.proxy.stats_cache_hits += 1
                    self.transport.sendto(cached_data, addr)
                    return

            self.proxy.stats_forwards += 1
            asyncio.create_task(self.forward_query_and_cache(data, addr, query_type))
        else:
            asyncio.create_task(self.forward_game_packet(data, addr))

    async def forward_query(self, data, addr):
        loop = asyncio.get_running_loop()
        temp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        temp_sock.setblocking(False)
        try:
            await loop.sock_connect(temp_sock, (self.proxy.backend_ip, self.proxy.backend_port))
            await loop.sock_sendall(temp_sock, data)
            response = await asyncio.wait_for(loop.sock_recv(temp_sock, 2048), timeout=1.0)
            self.transport.sendto(response, addr)
        except Exception as e:
            print(f"[Proxy] Query forward error: {e}")
        finally:
            temp_sock.close()

    async def forward_query_and_cache(self, data, addr, query_type):
        loop = asyncio.get_running_loop()
        temp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        temp_sock.setblocking(False)
        try:
            await loop.sock_connect(temp_sock, (self.proxy.backend_ip, self.proxy.backend_port))
            await loop.sock_sendall(temp_sock, data)
            response = await asyncio.wait_for(loop.sock_recv(temp_sock, 2048), timeout=1.0)
            self.proxy.cache[query_type] = (response, time.time())
            self.transport.sendto(response, addr)
        except Exception as e:
            print(f"[Proxy] Query cache forward error: {e}")
        finally:
            temp_sock.close()

    async def forward_game_packet(self, data, addr):
        now = time.time()
        if addr not in self.proxy.clients:
            loop = asyncio.get_running_loop()
            transport, protocol = await loop.create_datagram_endpoint(
                lambda: BackendProxyProtocol(self.proxy, addr),
                remote_addr=(self.proxy.backend_ip, self.proxy.backend_port)
            )
            self.proxy.clients[addr] = protocol
        
        protocol = self.proxy.clients[addr]
        protocol.last_active = now
        protocol.transport.sendto(data)

async def cleanup_clients(proxy):
    while True:
        await asyncio.sleep(10)
        print(f"[Proxy Stats] Cache Hits: {proxy.stats_cache_hits} | Forwards: {proxy.stats_forwards} | Blocked: {proxy.stats_blocked} | Active Clients: {len(proxy.clients)}")
        
        now = time.time()
        to_remove = []
        for addr, protocol in proxy.clients.items():
            if now - protocol.last_active > 60.0:
                print(f"[Proxy] Cleaning up inactive client mapping: {addr}")
                protocol.transport.close()
                to_remove.append(addr)
        for addr in to_remove:
            del proxy.clients[addr]

async def cleanup_bans(proxy):
    while True:
        await asyncio.sleep(1)
        now = time.time()
        to_unban = []
        for ip, expires in list(proxy.ban_list.items()):
            if now >= expires:
                to_unban.append(ip)
        for ip in to_unban:
            print(f"[Proxy] Ban expired for IP {ip}. Removing firewall rules.")
            del proxy.ban_list[ip]
            asyncio.create_task(remove_firewall_rule(ip))

async def main():
    config_path = os.path.join(os.path.dirname(__file__), 'config.json')
    proxy = QueryProxy(config_path)

    # Clean leftover firewall rules on startup
    await clear_existing_firewall_rules()

    loop = asyncio.get_running_loop()
    transport, protocol = await loop.create_datagram_endpoint(
        lambda: ProxyServerProtocol(proxy),
        local_addr=(proxy.proxy_ip, proxy.proxy_port)
    )

    asyncio.create_task(cleanup_clients(proxy))
    asyncio.create_task(cleanup_bans(proxy))

    try:
        await asyncio.Event().wait()
    except KeyboardInterrupt:
        pass
    finally:
        # Clean firewall rules on shutdown
        await clear_existing_firewall_rules()
        transport.close()

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("[Proxy] Shutting down.")
