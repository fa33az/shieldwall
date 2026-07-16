# ShieldWall DDoS Protection Suite
# Author: fa33az

import asyncio
import socket
import json
import time
import os

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

        self.cache = {}
        self.rate_limiter = {}
        self.ban_list = {}
        self.clients = {}

    def is_banned(self, ip):
        now = time.time()
        if ip in self.ban_list:
            if now < self.ban_list[ip]:
                return True
            else:
                del self.ban_list[ip]
        return False

    def check_rate_limit(self, ip):
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
            return False
        return True

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
                return

            query_type = data[10:11]
            
            if query_type == b'p':
                self.forward_query(data, addr)
                return

            now = time.time()
            if query_type in self.proxy.cache:
                cached_data, timestamp = self.proxy.cache[query_type]
                if now - timestamp <= self.proxy.cache_ttl:
                    self.transport.sendto(cached_data, addr)
                    return

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
        now = time.time()
        to_remove = []
        for addr, protocol in proxy.clients.items():
            if now - protocol.last_active > 60.0:
                print(f"[Proxy] Cleaning up inactive client mapping: {addr}")
                protocol.transport.close()
                to_remove.append(addr)
        for addr in to_remove:
            del proxy.clients[addr]

async def main():
    config_path = os.path.join(os.path.dirname(__file__), 'config.json')
    proxy = QueryProxy(config_path)

    loop = asyncio.get_running_loop()
    transport, protocol = await loop.create_datagram_endpoint(
        lambda: ProxyServerProtocol(proxy),
        local_addr=(proxy.proxy_ip, proxy.proxy_port)
    )

    asyncio.create_task(cleanup_clients(proxy))

    try:
        await asyncio.Event().wait()
    finally:
        transport.close()

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("[Proxy] Shutting down.")
