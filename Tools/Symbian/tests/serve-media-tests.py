#!/usr/bin/env python3
"""Local HTTP(S) fixtures for browser and native MMF acceptance; bounded routes only."""
import argparse
from functools import partial
import http.server
import json
from pathlib import Path
import re
import ssl
import threading
import time
from urllib.parse import parse_qs, urlsplit


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    def __init__(self, address, fixtures, reports):
        super().__init__(address, Handler)
        self.fixtures, self.reports = fixtures, reports
        self.states = {}
        self.state_lock = threading.Lock()


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def reply(self, status, body, mime="application/json", extra=()):
        self.send_response(status)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        for name, value in extra: self.send_header(name, value)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        try:
            self.get()
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            pass

    def get(self):
        url = urlsplit(self.path)
        query = parse_qs(url.query)
        token = query.get("token", [""])[0]
        if len(token) > 100 or (token and not re.fullmatch(r"[a-zA-Z0-9_-]+", token)):
            self.reply(400, b"bad token")
            return
        if url.path == "/ok":
            self.reply(200, b"ok", "text/plain")
            return
        if url.path == "/transfer-state":
            with self.server.state_lock:
                state = dict(self.server.states.get(token, {"started": False}))
            self.reply(200, json.dumps(state).encode())
            return
        if url.path == "/redirect-video.mp4":
            self.reply(302, b"", extra=[("Location", "/video.mp4")])
            return
        if url.path == "/media.html":
            self.reply(200, (Path(__file__).parent / "media.html").read_bytes(), "text/html; charset=utf-8")
            return
        name = url.path[1:]
        behavior = "normal"
        for prefix in ("slow-", "stall-", "delayed-", "truncated-"):
            if name.startswith(prefix):
                behavior, name = prefix[:-1], name[len(prefix):]
                break
        if name not in {"audio.wav", "audio.mp3", "video.mp4", "video-tail-index.mp4", "unsupported.mp4", "manifest.json"}:
            self.reply(404, b"missing fixture", "text/plain")
            return
        path = self.server.fixtures / name
        body = path.read_bytes()
        mime = {".wav": "audio/wav", ".mp3": "audio/mpeg", ".mp4": "video/mp4", ".json": "application/json"}[path.suffix]
        # Real byte-range support is needed for generic browsers to expose
        # seekability; the port's progressive loader still requests the file.
        range_header = self.headers.get("Range")
        if behavior in ("normal", "delayed") and range_header:
            match = re.fullmatch(r"bytes=(\d+)-(\d*)", range_header)
            if not match:
                self.reply(416, b"unsupported range", extra=[("Content-Range", f"bytes */{len(body)}")])
                return
            start = int(match[1]); end = int(match[2]) if match[2] else len(body)-1
            if start > end or start >= len(body):
                self.reply(416, b"invalid range", extra=[("Content-Range", f"bytes */{len(body)}")])
                return
            end = min(end, len(body)-1)
            if behavior == "delayed": time.sleep(2)
            self.reply(206, body[start:end+1], mime, [("Accept-Ranges", "bytes"), ("Content-Range", f"bytes {start}-{end}/{len(body)}")])
            return
        state = {"started": True, "sent": 0, "total": len(body), "complete": False, "cancelled": False, "stalling": False}
        def publish():
            if token:
                with self.server.state_lock:
                    self.server.states[token] = dict(state)
        publish()
        if behavior == "delayed": time.sleep(2)
        self.send_response(200)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        if behavior in ("normal", "delayed"):
            self.send_header("Accept-Ranges", "bytes")
        self.end_headers()
        limit = len(body) // 3 if behavior == "truncated" else len(body)
        try:
            for offset in range(0, limit, 16384):
                chunk = body[offset:min(offset + 16384, limit)]
                self.wfile.write(chunk)
                self.wfile.flush()
                state["sent"] += len(chunk)
                publish()
                if behavior == "stall" and offset < 131072 <= offset + len(chunk):
                    state["stalling"] = True; publish()
                    time.sleep(6)
                    state["stalling"] = False; publish()
                if behavior == "slow": time.sleep(.18)
                elif behavior == "stall": time.sleep(.03)
            state["complete"] = behavior != "truncated"
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            state["cancelled"] = True
        finally:
            publish()
        if behavior == "truncated": self.close_connection = True

    def do_POST(self):
        if urlsplit(self.path).path != "/report":
            self.reply(404, b"missing")
            return
        length = int(self.headers.get("Content-Length", "0"))
        if length < 1 or length > 131072:
            self.reply(413, b"invalid report size")
            return
        try:
            report = json.loads(self.rfile.read(length))
            run = report["run"]
            if report.get("suite") != "media" or not re.fullmatch(r"[a-zA-Z0-9_-]{1,100}", run): raise ValueError()
            destination = self.server.reports / (run + ".json")
            with destination.open("x") as output: json.dump(report, output, indent=2)
        except (ValueError, TypeError, KeyError, FileExistsError):
            self.reply(400, b"bad or duplicate report")
            return
        self.reply(200, b"saved", "text/plain")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--port", type=int, default=8775)
    parser.add_argument("--tls-dir", type=Path)
    parser.add_argument("--tls-port", type=int, default=8776)
    args = parser.parse_args()
    args.reports.mkdir(parents=True, exist_ok=True)
    servers = [Server(("127.0.0.1", args.port), args.fixtures, args.reports)]
    if args.tls_dir:
        for index, name in enumerate(("good", "wrong", "untrusted")):
            instance = Server(("127.0.0.1", args.tls_port + index), args.fixtures, args.reports)
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(args.tls_dir / (name + ".crt"), args.tls_dir / (name + ".key"))
            instance.socket = context.wrap_socket(instance.socket, server_side=True)
            servers.append(instance)
    threads = [threading.Thread(target=server.serve_forever, daemon=True) for server in servers]
    for thread in threads: thread.start()
    print("Media listeners ready: " + ", ".join(str(server.server_port) for server in servers), flush=True)
    try:
        while True: time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        for server in servers: server.shutdown(); server.server_close()
        for thread in threads: thread.join()


if __name__ == "__main__":
    main()
