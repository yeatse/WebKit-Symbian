#!/usr/bin/env python3
"""Local download fixtures; records request count/body without storing files."""
import argparse
import gzip
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import ssl
import threading
import time
from urllib.parse import parse_qs, quote, urlsplit

BLOCK = bytes(range(256)) * 16
PAYLOAD = BLOCK * 128
PAYLOAD_SHA256 = hashlib.sha256(PAYLOAD).hexdigest()


class FixtureServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, log, peer_port):
        super().__init__(address, Handler)
        self.log_path = log
        self.peer_port = peer_port
        self.log_lock = threading.Lock()

    def record(self, value):
        value["monotonic_ns"] = time.monotonic_ns()
        with self.log_lock, self.log_path.open("a", encoding="utf-8") as output:
            output.write(json.dumps(value, ensure_ascii=True) + "\n")


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_):
        pass

    def do_GET(self):
        self.respond()

    def do_POST(self):
        self.respond()

    def respond(self):
        parts = urlsplit(self.path)
        query = parse_qs(parts.query)
        token = query.get("run", [""])[0]
        if not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", token):
            self.send_error(400, "a unique alphanumeric run token is required")
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self.send_error(400)
            return
        if length < 0 or length > 4096:
            self.send_error(413)
            return
        body = self.rfile.read(length)
        self.server.record({"event": "request", "run": token, "method": self.command,
                            "path": parts.path, "body_bytes": len(body),
                            "body_sha256": hashlib.sha256(body).hexdigest(),
                            "authorization_present": "Authorization" in self.headers,
                            "cookie_present": "Cookie" in self.headers})
        path = parts.path
        if path == "/":
            self.send_index(token)
            return
        if path in ("/redirect", "/redirect-other-port", "/redirect-file", "/redirect-loop"):
            targets = {"/redirect": f"/attachment?run={token}",
                       "/redirect-other-port": f"http://localhost:{self.server.peer_port}/attachment?run={token}",
                       "/redirect-file": "file:///C:/Data/private-download-test.bin",
                       "/redirect-loop": f"/redirect-loop?run={token}"}
            self.send_response(302)
            self.send_header("Location", targets[path])
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if path == "/not-found":
            self.send_error(404, "intentional fixture error")
            return
        allowed = {"/attachment", "/attribute", "/post-attachment", "/unicode", "/unsafe-name",
                   "/chunked", "/slow", "/gzip", "/truncated", "/too-large", "/partial"}
        if path not in allowed:
            self.send_error(404)
            return
        if path == "/post-attachment" and self.command != "POST":
            self.send_error(405)
            return
        self.send_response(206 if path == "/partial" else 200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Cache-Control", "no-store")
        if path != "/attribute":
            name = "payload.bin"
            if path == "/unicode":
                self.send_header("Content-Disposition", "attachment; filename*=UTF-8''" + quote("报告 😀.bin"))
            else:
                if path == "/unsafe-name":
                    name = "../../private/evil?.bin"
                self.send_header("Content-Disposition", f'attachment; filename="{name}"')
        payload = gzip.compress(PAYLOAD, mtime=0) if path == "/gzip" else PAYLOAD
        if path == "/gzip":
            self.send_header("Content-Encoding", "gzip")
        if path in ("/chunked", "/slow"):
            self.send_header("Transfer-Encoding", "chunked")
        else:
            self.send_header("Content-Length", str(0x80000000 if path == "/too-large" else len(payload)))
        if path in ("/truncated", "/too-large"):
            self.send_header("Connection", "close")
        if path == "/partial":
            self.send_header("Content-Range", f"bytes 0-{len(payload)-1}/{len(payload)*2}")
        self.end_headers()
        written = 0
        try:
            if path == "/too-large":
                self.close_connection = True
            elif path == "/truncated":
                self.wfile.write(payload[:4096])
                written = 4096
                self.close_connection = True
            elif path in ("/chunked", "/slow"):
                for offset in range(0, len(payload), 4096):
                    chunk = payload[offset:offset + 4096]
                    self.wfile.write(f"{len(chunk):x}\r\n".encode() + chunk + b"\r\n")
                    self.wfile.flush()
                    written += len(chunk)
                    if path == "/slow":
                        time.sleep(0.05)
                self.wfile.write(b"0\r\n\r\n")
            else:
                self.wfile.write(payload)
                written = len(payload)
            self.wfile.flush()
            self.server.record({"event": "sent", "run": token, "path": path, "wire_body_bytes": written,
                                "decoded_sha256": PAYLOAD_SHA256})
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            self.close_connection = True
            self.server.record({"event": "disconnected", "run": token, "path": path, "wire_body_bytes": written})

    def send_index(self, token):
        cases = [("attachment", "Attachment navigation"), ("attribute", "Download attribute"),
                 ("unicode", "Unicode filename"), ("unsafe-name", "Untrusted filename"),
                 ("chunked", "Chunked body"), ("gzip", "Compressed body"),
                 ("slow", "Slow transfer: cancel"), ("redirect", "Same-origin redirect"),
                 ("redirect-other-port", "Cross-port redirect"), ("redirect-file", "Forbidden file redirect"),
                 ("redirect-loop", "Redirect limit"), ("not-found", "HTTP 404"),
                 ("truncated", "Truncated body"), ("too-large", "Over native size limit"),
                 ("partial", "Unsolicited partial response")]
        links = ""
        for path, label in cases:
            # These four exercise response-policy conversion; the others begin
            # through download=, including explicit unsuccessful responses.
            attribute = "" if path in {"attachment", "unicode", "unsafe-name", "redirect"} else ' download="link-suggestion.bin"'
            links += f'<li><a href="/{path}?run={token}"{attribute}>{label}</a></li>'
        data = (f'<!doctype html><meta charset="utf-8"><title>Download fixtures</title>'
                f'<h1>Download fixtures</h1><p>Run {token}. Every successful payload is 524288 bytes, SHA-256 {PAYLOAD_SHA256}.</p>'
                f'<p>These links do not mark tests PASS. Verify actual native UI, destination bytes, cleanup and request logs.</p>'
                f'<ol>{links}</ol><form method="post" action="/post-attachment?run={token}">'
                '<input type="hidden" name="token" value="post-once"><button>POST attachment: exactly one request</button></form>').encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8800)
    parser.add_argument("--peer-port", type=int, default=8801)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--certificate", type=Path)
    parser.add_argument("--key", type=Path)
    args = parser.parse_args()
    if bool(args.certificate) != bool(args.key):
        parser.error("certificate and key must be supplied together")
    args.log.parent.mkdir(parents=True, exist_ok=True)
    server = FixtureServer(("127.0.0.1", args.port), args.log, args.peer_port)
    if args.certificate:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(args.certificate, args.key)
        server.socket = context.wrap_socket(server.socket, server_side=True)
    print(json.dumps({"port": server.server_port, "payload_bytes": len(PAYLOAD), "payload_sha256": PAYLOAD_SHA256,
                      "tls": bool(args.certificate)}), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
