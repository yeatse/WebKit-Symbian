#!/usr/bin/env python3
"""Local HTTP/WebSocket/TLS fixtures for the Symbian browser. No external services."""
import argparse
import base64
import hashlib
import http.server
import json
from pathlib import Path
import socket
import ssl
import struct
import threading
import time
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parent


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, format, *args):
        print(f"[{self.server.server_port}] {format % args}", flush=True)

    def response(self, body=b"ok", status=200, headers=()):
        if isinstance(body, str):
            body = body.encode()
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        origin = self.headers.get("Origin", "")
        if urlsplit(origin).hostname in ("localhost", "127.0.0.1"):
            self.send_header("Access-Control-Allow-Origin", origin)
            self.send_header("Access-Control-Allow-Credentials", "true")
        for name, value in headers:
            self.send_header(name, value)
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_OPTIONS(self):
        self.response(headers=(("Access-Control-Allow-Methods", "GET, POST, OPTIONS"),
                               ("Access-Control-Allow-Headers", "Authorization, Content-Type")))

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        if length > 1024 * 1024:
            self.response(status=413)
            self.close_connection = True
            return
        self.body = self.rfile.read(length)
        self.do_GET()

    def do_GET(self):
        parsed = urlsplit(self.path)
        query = parse_qs(parsed.query)
        path = parsed.path
        if self.headers.get("Upgrade", "").lower() == "websocket":
            return self.websocket()
        if path in ("/", "/network.html", "/tls.html", "/webcrypto.html"):
            file = ROOT / (path.lstrip("/") or "network.html")
            if file.is_file():
                return self.response(file.read_bytes(), headers=(("Content-Type", "text/html; charset=utf-8"),))
            return self.response(status=404)
        if path == "/report":
            try:
                report = json.loads(getattr(self, "body", b""))
            except (ValueError, UnicodeError):
                return self.response(status=400)
            name = f"{time.time_ns()}-{self.server.server_port}.json"
            (self.server.reports / name).write_text(json.dumps(report, ensure_ascii=False, indent=2))
            print("REPORT " + json.dumps(report, ensure_ascii=False), flush=True)
            return self.response()
        if path == "/auth":
            return self.response("unauthorized", 401, (("WWW-Authenticate", 'Basic realm="fixture"'),))
        if path == "/slow":
            time.sleep(2)
            return self.response("late")
        if path == "/chunked":
            self.send_response(200)
            self.send_header("Transfer-Encoding", "chunked")
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            try:
                for chunk in (b"chunk", b"ed", b"-response"):
                    self.wfile.write(f"{len(chunk):x}\r\n".encode() + chunk + b"\r\n")
                    self.wfile.flush()
                    time.sleep(0.05)
                self.wfile.write(b"0\r\n\r\n")
            except (BrokenPipeError, ConnectionResetError):
                pass
            return
        if path == "/redirect":
            destination = query.get("to", ["/echo"])[0]
            code = int(query.get("status", ["302"])[0])
            if "\r" in destination or "\n" in destination or code not in (302, 303, 307):
                return self.response(status=400)
            return self.response(status=code, headers=(("Location", destination),))
        if path == "/cookie":
            name = query.get("name", ["fixture"])[0]
            value = query.get("value", ["yes"])[0]
            cookie_path = query.get("path", ["/"])[0]
            if any(c in name + value + cookie_path for c in "\r\n;"):
                return self.response(status=400)
            cookie = f"{name}={value}; Path={cookie_path}; SameSite=Lax"
            if "delete" in query:
                cookie += "; Max-Age=0"
            return self.response(headers=(("Set-Cookie", cookie),))
        if path.endswith("/echo"):
            return self.response(json.dumps({"method": self.command,
                "body": getattr(self, "body", b"").decode(errors="replace"),
                "cookie": self.headers.get("Cookie", ""),
                "authorization": self.headers.get("Authorization", ""),
                "contentType": self.headers.get("Content-Type", ""),
                "path": path}), headers=(("Content-Type", "application/json"),))
        if path == "/ok":
            return self.response("ok")
        return self.response(status=404)

    def websocket(self):
        if urlsplit(self.path).path != "/ws" or self.headers.get("Sec-WebSocket-Version") != "13":
            return self.response(status=400)
        key = self.headers.get("Sec-WebSocket-Key", "")
        try:
            if len(base64.b64decode(key, validate=True)) != 16:
                return self.response(status=400)
        except ValueError:
            return self.response(status=400)
        accept = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
        self.send_response(101)
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", accept)
        self.end_headers()
        self.connection.settimeout(15)
        self.close_connection = True

        def exact(length):
            result = self.rfile.read(length)
            if len(result) != length:
                raise EOFError()
            return result

        def frame(opcode, payload):
            length = len(payload)
            header = bytes([0x80 | opcode, length]) if length < 126 else bytes([0x80 | opcode, 126]) + struct.pack("!H", length)
            self.wfile.write(header + payload)
            self.wfile.flush()

        try:
            while True:
                first, second = exact(2)
                opcode = first & 15
                if not first & 0x80 or first & 0x70 or not second & 0x80:
                    return
                length = second & 127
                if length == 126:
                    length = struct.unpack("!H", exact(2))[0]
                elif length == 127:
                    length = struct.unpack("!Q", exact(8))[0]
                if length > 65535:
                    return
                mask = exact(4)
                payload = bytes(byte ^ mask[index % 4] for index, byte in enumerate(exact(length)))
                if opcode == 8:
                    frame(8, payload)
                    return
                if opcode == 9:
                    frame(10, payload)
                elif opcode in (1, 2):
                    frame(opcode, payload)
        except (EOFError, OSError, socket.timeout):
            pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--ports", type=int, nargs=2, default=[8765, 8766])
    parser.add_argument("--reports", type=Path, default=Path("network-reports"))
    parser.add_argument("--tls-dir", type=Path, help="Directory containing good/wrong/untrusted .crt and .key files")
    args = parser.parse_args()
    args.reports.mkdir(parents=True, exist_ok=True)
    servers = []
    endpoints = [(port, None) for port in args.ports]
    if args.tls_dir:
        endpoints += [(8767, "good"), (8768, "wrong"), (8769, "untrusted")]
    try:
        for port, certificate in endpoints:
            server = Server((args.bind, port), Handler)
            server.reports = args.reports
            if certificate:
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                context.load_cert_chain(args.tls_dir / (certificate + ".crt"), args.tls_dir / (certificate + ".key"))
                server.socket = context.wrap_socket(server.socket, server_side=True)
            servers.append(server)
            threading.Thread(target=server.serve_forever, daemon=True).start()
            print(f"READY {'https' if certificate else 'http'}://{args.bind}:{port}", flush=True)
        threading.Event().wait()
    except KeyboardInterrupt:
        pass
    finally:
        for server in servers:
            server.shutdown()
            server.server_close()


if __name__ == "__main__":
    main()
