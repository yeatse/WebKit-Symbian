#!/usr/bin/env python3
"""Local, genuinely cross-site cookie fixture; no browser or emulator launcher."""

import argparse
import base64
import hashlib
import http.server
import json
import re
import ssl
import struct
import threading
import time
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parent
RUN = re.compile(r"[A-Za-z0-9_-]{1,48}\Z")
POLICIES = {
    "strict": "; SameSite=Strict",
    "lax": "; SameSite=Lax",
    "none": "; SameSite=None",
    "default": "",
    "mixed": "; sAmEsItE=sTrIcT",
    "unknown": "; SameSite=Invalid",
    "duplicate": "; SameSite=None; SameSite=Lax",
}


class State:
    def __init__(self, output, https_port, other_port, http_port):
        self.output = Path(output)
        self.output.mkdir(parents=True, exist_ok=True)
        self.https_port, self.other_port, self.http_port = (
            https_port,
            other_port,
            http_port,
        )
        self.lock = threading.Lock()
        self.runs = {}

    def config(self, run):
        return {
            "run": run,
            "a": f"https://localhost:{self.https_port}",
            "b": f"https://127.0.0.1:{self.https_port}",
            "sameSiteOtherPort": f"https://localhost:{self.other_port}",
            "httpA": f"http://localhost:{self.http_port}",
        }

    def append(self, run, kind, value):
        with self.lock:
            path = self.output / (run + ".json")
            if run not in self.runs:
                self.runs[run] = (
                    json.loads(path.read_text())
                    if path.exists()
                    else {
                        "suite": "cookie-samesite",
                        "config": self.config(run),
                        "requests": [],
                        "results": [],
                    }
                )
            data = self.runs[run]
            data[kind].append(value)
            temporary = path.with_suffix(".tmp")
            temporary.write_text(json.dumps(data, indent=2) + "\n")
            temporary.replace(path)

    def results(self, run):
        with self.lock:
            path = self.output / (run + ".json")
            return json.loads(path.read_text())["results"] if path.exists() else []


def cookie_headers(run):
    prefix = "ss_" + run + "_"
    result = [
        ("Set-Cookie", prefix + key + "=1; Path=/; Max-Age=3600; Secure" + policy)
        for key, policy in POLICIES.items()
    ]
    result += [
        ("Set-Cookie", prefix + "session=1; Path=/; Secure; SameSite=Strict"),
        (
            "Set-Cookie",
            prefix
            + "httpOnly=1; Path=/; Max-Age=3600; Secure; HttpOnly; SameSite=Strict",
        ),
        ("Set-Cookie", prefix + "badnone=1; Path=/; Max-Age=3600; SameSite=None"),
    ]
    return result


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        print(f"[{self.server.server_port}] {fmt % args}", flush=True)

    def response(self, body="ok", status=200, headers=()):
        if isinstance(body, str):
            body = body.encode()
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        origin = self.headers.get("Origin", "")
        allowed = self.server.state.config("unused")
        if origin in (
            allowed["a"],
            allowed["b"],
            allowed["sameSiteOtherPort"],
            allowed["httpA"],
        ):
            self.send_header("Access-Control-Allow-Origin", origin)
            self.send_header("Access-Control-Allow-Credentials", "true")
        # Redirected CORS requests may legitimately have an opaque origin.
        elif origin == "null":
            self.send_header("Access-Control-Allow-Origin", "null")
            self.send_header("Access-Control-Allow-Credentials", "true")
        for name, value in headers:
            self.send_header(name, value)
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_OPTIONS(self):
        self.response(
            headers=(
                ("Access-Control-Allow-Methods", "GET, POST, OPTIONS"),
                ("Access-Control-Allow-Headers", "Content-Type"),
            )
        )

    def do_POST(self):
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            return self.response(status=400)
        if length < 0 or length > 65536:
            self.close_connection = True
            return self.response(status=413)
        self.body = self.rfile.read(length)
        self.do_GET()

    def page(self, run, phase, observed, extra=None, headers=()):
        config = self.server.state.config(run)
        config.update(phase=phase, observed=observed, extra=extra)
        encoded = json.dumps(config).replace("<", "\\u003c")
        return self.response(
            '<!doctype html><meta charset="utf-8"><title>SameSite running</title>'
            '<h1>SameSite regression</h1><pre id="output"></pre><script>const config='
            + encoded
            + ';</script><script src="/case.js"></script>',
            headers=(("Content-Type", "text/html; charset=utf-8"), *headers),
        )

    def do_GET(self):
        parsed = urlsplit(self.path)
        if parsed.path == "/case.js":
            return self.response(
                (ROOT / "case.js").read_bytes(),
                headers=(("Content-Type", "text/javascript"),),
            )
        query = parse_qs(parsed.query)
        run = query.get("run", [""])[0]
        if not RUN.fullmatch(run):
            return self.response(
                "run must contain 1..48 letters, digits, underscores or hyphens", 400
            )
        observed = {
            "path": parsed.path,
            "method": self.command,
            "host": self.headers.get("Host", ""),
            "cookie": self.headers.get("Cookie", ""),
            "origin": self.headers.get("Origin", ""),
            "query": {key: values[-1] for key, values in query.items()},
            "tls": isinstance(self.connection, ssl.SSLSocket),
            "time_ns": time.time_ns(),
        }
        state = self.server.state
        state.append(run, "requests", observed)
        if parsed.path == "/record" and self.command == "POST":
            try:
                results = json.loads(self.body)
                if not isinstance(results, list) or len(results) > 100:
                    raise ValueError()
                for result in results:
                    if result.get("status") not in ("PASS", "FAIL") or not isinstance(
                        result.get("name"), str
                    ):
                        raise ValueError()
                for result in results:
                    state.append(run, "results", result)
            except (ValueError, AttributeError, UnicodeError):
                return self.response(status=400)
            return self.response()
        if parsed.path in ("/start", "/seed-b"):
            return self.page(
                run,
                "seed-a" if parsed.path == "/start" else "seed-b",
                observed,
                headers=cookie_headers(run),
            )
        if parsed.path in ("/main", "/resume", "/resume-b", "/done"):
            return self.page(run, parsed.path[1:], observed)
        if parsed.path == "/summary":
            return self.response(
                json.dumps(state.results(run)),
                headers=(("Content-Type", "application/json"),),
            )
        if parsed.path == "/nav":
            return self.page(run, "nav", observed, query.get("step", [""])[0])
        if parsed.path in ("/frame", "/nested-parent", "/nested-child"):
            return self.page(run, parsed.path[1:], observed)
        if parsed.path == "/echo":
            return self.response(
                json.dumps(observed), headers=(("Content-Type", "application/json"),)
            )
        if parsed.path == "/set-response":
            # SameSite=Lax/Strict/default from a cross-site subresource must not be stored.
            headers = [
                (name, value.replace("ss_" + run + "_", "ss_" + run + "_response_"))
                for name, value in cookie_headers(run)
            ]
            return self.response(
                json.dumps(observed),
                headers=(("Content-Type", "application/json"), *headers),
            )
        if parsed.path == "/redirect":
            target = query.get("to", [""])[0]
            code = query.get("code", ["302"])[0]
            allowed = state.config(run)
            if (
                code not in ("302", "303", "307", "308")
                or any(c in target for c in "\r\n")
                or not any(
                    target.startswith(allowed[k] + "/")
                    for k in ("a", "b", "sameSiteOtherPort", "httpA")
                )
            ):
                return self.response(status=400)
            return self.response(status=int(code), headers=(("Location", target),))
        if parsed.path == "/ws":
            return self.websocket(observed)
        return self.response(status=404)

    def websocket(self, observed):
        key = self.headers.get("Sec-WebSocket-Key", "")
        try:
            if (
                self.headers.get("Upgrade", "").lower() != "websocket"
                or self.headers.get("Sec-WebSocket-Version") != "13"
                or len(base64.b64decode(key, validate=True)) != 16
            ):
                return self.response(status=400)
        except ValueError:
            return self.response(status=400)
        accept = base64.b64encode(
            hashlib.sha1(
                (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()
            ).digest()
        ).decode()
        self.send_response(101)
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", accept)
        self.end_headers()
        message = json.dumps(observed).encode()
        self.wfile.write(
            b"\x81"
            + (
                bytes([len(message)])
                if len(message) < 126
                else b"\x7e" + struct.pack("!H", len(message))
            )
            + message
        )
        # Valid normal-close frame. No client payload is required by this fixture.
        self.wfile.write(b"\x88\x02\x03\xe8")
        self.wfile.flush()
        self.close_connection = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tls", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--https-port", type=int, default=8791)
    parser.add_argument("--other-port", type=int, default=8792)
    parser.add_argument("--http-port", type=int, default=8790)
    args = parser.parse_args()
    if len({args.https_port, args.other_port, args.http_port}) != 3:
        parser.error("three distinct ports are required")
    state = State(args.output, args.https_port, args.other_port, args.http_port)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(args.tls / "server.crt", args.tls / "server.key")
    servers = []
    try:
        for port in (args.https_port, args.other_port, args.http_port):
            server = Server(("127.0.0.1", port), Handler)
            server.state = state
            if port != args.http_port:
                server.socket = context.wrap_socket(server.socket, server_side=True)
            threading.Thread(target=server.serve_forever, daemon=True).start()
            servers.append(server)
        print(json.dumps(state.config("CHOOSE_NEW_RUN_ID")), flush=True)
        threading.Event().wait()
    except KeyboardInterrupt:
        pass
    finally:
        for server in servers:
            server.shutdown()
            server.server_close()


if __name__ == "__main__":
    main()
