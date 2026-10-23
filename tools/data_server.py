#!/usr/bin/env python3
"""Serves the converted v83 NX files to the iOS client on first launch.

GET /manifest.json      -> {"files": [{"name": "Map.nx", "size": N, "sha256": H, "mtime": T}, ...]}
                           (Base.nx last; T = the file's modification time, newer wins on the phone)
GET /<Name>.nx          -> the file, with Range support so interrupted downloads resume.

Only the NX names listed below are served, and only to private-network clients (LAN, Tailscale,
localhost): this is your own game data and must never be reachable from the internet.
Usage: tools/data_server.py [port]   (default 8485; run it from a LaunchAgent: tools/maplev83-data.plist.example)
       tools/data_server.py --print-manifest   (what tools/build_ios.sh bundles as nx/manifest.json)
"""
import hashlib
import ipaddress
import json
import os
import re
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

NX_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "nx")
# Base.nx last: the client only treats the set as complete once every file exists.
FILES = ["Character", "Effect", "Etc", "Item", "Map", "Mob", "Morph", "Npc", "Quest", "Reactor",
         "Skill", "Sound", "String", "TamingMob", "UI", "Base"]
ALLOWED = [ipaddress.ip_network(n) for n in ("127.0.0.0/8", "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16",
                                             "100.64.0.0/10", "::1/128", "fc00::/7")]
CHUNK = 1 << 20
HASH_CACHE = os.path.join(NX_DIR, ".manifest-cache.json")
hash_lock = threading.Lock()


def manifest():
    """Every file's size, SHA-256 and mtime. Hashes are cached by (size, mtime_ns), so only a
    re-converted file is read again. None when the converted set is incomplete."""
    with hash_lock:
        try:
            with open(HASH_CACHE) as f:
                cache = json.load(f)
        except (OSError, ValueError):
            cache = {}
        entries, changed = [], False
        for name in FILES:
            path = os.path.join(NX_DIR, name + ".nx")
            try:
                st = os.stat(path)
            except OSError:
                return None
            key = "%d:%d" % (st.st_size, st.st_mtime_ns)
            cached = cache.get(name)
            if not cached or cached.get("key") != key:
                digest = hashlib.sha256()
                with open(path, "rb") as f:
                    for block in iter(lambda: f.read(8 * CHUNK), b""):
                        digest.update(block)
                cached = cache[name] = {"key": key, "sha256": digest.hexdigest()}
                changed = True
            entries.append({"name": name + ".nx", "size": st.st_size, "sha256": cached["sha256"],
                            "mtime": int(st.st_mtime)})
        if changed:
            tmp = HASH_CACHE + ".tmp"
            with open(tmp, "w") as f:
                json.dump(cache, f)
            os.replace(tmp, HASH_CACHE)
        return {"files": entries}


def allowed(host):
    try:
        ip = ipaddress.ip_address(host.removeprefix("::ffff:"))
    except ValueError:
        return False
    return any(ip in net for net in ALLOWED)


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        sys.stderr.write("%s %s\n" % (self.client_address[0], fmt % args))

    def fail(self, code):
        self.send_response(code)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_HEAD(self):
        self.handle_request(head=True)

    def do_GET(self):
        self.handle_request(head=False)

    def handle_request(self, head):
        if not allowed(self.client_address[0]):
            return self.fail(403)

        if self.path == "/manifest.json":
            current = manifest()
            if current is None:
                return self.fail(503)
            body = json.dumps(current).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            if not head:
                self.wfile.write(body)
            return

        match = re.fullmatch(r"/([A-Za-z]+)\.nx", self.path)
        if not match or match.group(1) not in FILES:
            return self.fail(404)

        path = os.path.join(NX_DIR, match.group(1) + ".nx")
        try:
            f = open(path, "rb")
        except OSError:
            return self.fail(404)

        with f:
            size = os.fstat(f.fileno()).st_size
            start, end = 0, size - 1
            ranged = self.headers.get("Range")
            if ranged:
                m = re.fullmatch(r"bytes=(\d+)-(\d*)", ranged.strip())
                if not m or int(m.group(1)) >= size:
                    self.send_response(416)
                    self.send_header("Content-Range", "bytes */%d" % size)
                    self.send_header("Content-Length", "0")
                    self.end_headers()
                    return
                start = int(m.group(1))
                if m.group(2):
                    end = min(int(m.group(2)), size - 1)

            length = end - start + 1
            self.send_response(206 if ranged else 200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(length))
            self.send_header("Accept-Ranges", "bytes")
            if ranged:
                self.send_header("Content-Range", "bytes %d-%d/%d" % (start, end, size))
            self.end_headers()
            if head:
                return

            f.seek(start)
            remaining = length
            try:
                while remaining > 0:
                    sent = os.sendfile(self.wfile.fileno(), f.fileno(), start + length - remaining,
                                       min(remaining, CHUNK))
                    if sent == 0:
                        break
                    remaining -= sent
            except (BrokenPipeError, ConnectionResetError):
                pass


class Server(ThreadingHTTPServer):
    daemon_threads = True
    address_family = __import__("socket").AF_INET6

    def server_bind(self):
        import socket
        self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        super().server_bind()


if __name__ == "__main__":
    if sys.argv[1:] == ["--print-manifest"]:
        current = manifest()
        if current is None:
            sys.exit("data/nx is incomplete - run tools/convert_data.sh first")
        print(json.dumps(current, indent=1))
        sys.exit(0)
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8485
    threading.Thread(target=manifest, daemon=True).start()  # warm the hash cache
    Server(("::", port), Handler).serve_forever()
