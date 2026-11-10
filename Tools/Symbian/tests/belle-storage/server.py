#!/usr/bin/env python3
"""Serve two local HTTP origins and retain immutable, token-specific results."""
import argparse
import http.server
import json
from pathlib import Path
import signal
import threading
from urllib.parse import urlsplit

from contract import identity

ROOT = Path(__file__).resolve().parent
FILES = {"/": ("index.html", "text/html"), "/index.html": ("index.html", "text/html"),
         "/suite.js": ("suite.js", "text/javascript"), "/peer.html": ("peer.html", "text/html"),
         "/peer.js": ("peer.js", "text/javascript"), "/worker.js": ("worker.js", "text/javascript")}


class Handler(http.server.BaseHTTPRequestHandler):
    def reply(self, code, body, mime="application/json"):
        self.send_response(code)
        self.send_header("Content-Type", mime + "; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        # Deliberately no CORS opt-in: the second port remains a distinct origin.
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = urlsplit(self.path).path
        if path == "/config.json":
            self.reply(200, json.dumps({"otherOrigin": self.server.other_origin}).encode())
        elif path in FILES:
            filename, mime = FILES[path]
            self.reply(200, (self.server.fixtures / filename).read_bytes(), mime)
        else:
            self.reply(404, b"not found", "text/plain")

    def do_POST(self):
        if urlsplit(self.path).path != "/report":
            self.reply(404, b"not found"); return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 1 <= length <= 65536:
                raise ValueError("invalid report length")
            body = self.rfile.read(length)
            report = json.loads(body)
            token, phase = identity(report, self.server.suite)
            with (self.server.reports / (token + "-" + phase + ".json")).open("xb") as output:
                output.write(body + b"\n")
        except FileExistsError:
            self.reply(409, b"report already exists"); return
        except (ValueError, TypeError, OSError) as error:
            self.reply(400, str(error).encode()); return
        self.reply(201, b'{"stored":true}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--host", default="localhost", help="Hostname visible to the browser")
    parser.add_argument("--suite", choices=("storage", "backends"), default="storage")
    parser.add_argument("--port", type=int)
    parser.add_argument("--other-port", type=int)
    args = parser.parse_args()
    args.port = args.port or (8780 if args.suite == "storage" else 8784)
    args.other_port = args.other_port or (8781 if args.suite == "storage" else 8785)
    if args.port == args.other_port: parser.error("origins need distinct ports")
    args.reports.mkdir(parents=True, exist_ok=True)
    servers = []
    stopped = threading.Event()
    signal.signal(signal.SIGINT, lambda *_: stopped.set())
    signal.signal(signal.SIGTERM, lambda *_: stopped.set())
    try:
        for port, other in ((args.port, args.other_port), (args.other_port, args.port)):
            server = http.server.ThreadingHTTPServer((args.bind, port), Handler)
            server.daemon_threads = True
            server.reports = args.reports
            server.suite = args.suite
            server.fixtures = ROOT.parent / ("belle-" + args.suite)
            server.other_origin = f"http://{args.host}:{other}"
            servers.append(server)
            threading.Thread(target=server.serve_forever, daemon=True).start()
        print(f"READY http://{args.host}:{args.port}/index.html and http://{args.host}:{args.other_port}", flush=True)
        stopped.wait()
    finally:
        for server in servers: server.shutdown(); server.server_close()


if __name__ == "__main__":
    main()
