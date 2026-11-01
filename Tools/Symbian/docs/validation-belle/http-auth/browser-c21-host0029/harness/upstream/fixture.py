#!/usr/bin/env python3
"""Ephemeral localhost-only Basic/Digest browser acceptance fixture.

Only synthetic test credentials are accepted. Request headers and credential
values are never included in diagnostic output or evidence.
"""

import argparse
import base64
import hashlib
import hmac
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from pathlib import Path
import re
import secrets
import time
from urllib.parse import parse_qs, urlsplit
from urllib.request import parse_http_list, parse_keqv_list

CASES = ("success", "cancel-user", "cancel-password", "retry", "navigate-user",
         "navigate-password", "exit-user", "exit-password")
EVENTS = {"ready", "started", "loaded", "exhausted", "navigated", "responsive"}


def private_json(path, data):
    with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), "w") as stream:
        json.dump(data, stream)
        stream.write("\n")


def md5(text):
    return hashlib.md5(text.encode("ascii")).hexdigest()


class State:
    def __init__(self, run, scheme, case, username, password, evidence):
        if not re.fullmatch(r"[a-zA-Z0-9_-]{8,80}", run):
            raise ValueError("run token must contain 8–80 ASCII letters, digits, underscores or hyphens")
        if scheme not in ("basic", "digest") or case not in CASES:
            raise ValueError("unknown scheme/case")
        self.run, self.scheme, self.case = run, scheme, case
        self.username, self.password = username, password
        self.realm = "Symbian fixture " + run
        self.nonce = secrets.token_hex(20)
        self.evidence = evidence
        self.events = []
        self.command = "wait"
        self.record("fixture-ready")

    def record(self, event, **fields):
        entry = {"sequence": len(self.events) + 1, "run": self.run,
                 "scheme": self.scheme, "case": self.case, "event": event,
                 "monotonic": time.monotonic(), **fields}
        # Callers supply only finite enums, booleans and numbers, never headers.
        self.events.append(entry)
        with self.evidence.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(entry, sort_keys=True) + "\n")
        return entry

    def authenticate(self, header, method, path):
        if not header or len(header) > 4096:
            return False
        try:
            if self.scheme == "basic":
                scheme, value = header.split(" ", 1)
                decoded = base64.b64decode(value, validate=True)
                return scheme.lower() == "basic" and hmac.compare_digest(
                    decoded, (self.username + ":" + self.password).encode("ascii"))
            scheme, value = header.split(" ", 1)
            fields = parse_keqv_list(parse_http_list(value))
            if scheme.lower() != "digest" or any(fields.get(key) != expected for key, expected in {
                "username": self.username, "realm": self.realm, "nonce": self.nonce,
                "uri": path, "qop": "auth"}.items()):
                return False
            if fields.get("algorithm", "MD5").upper() != "MD5":
                return False
            if not re.fullmatch(r"[0-9a-fA-F]{8}", fields.get("nc", "")) or not fields.get("cnonce"):
                return False
            expected = md5(":".join((md5(self.username + ":" + self.realm + ":" + self.password),
                self.nonce, fields["nc"], fields["cnonce"], "auth", md5(method + ":" + path))))
            return hmac.compare_digest(fields.get("response", ""), expected)
        except (ValueError, UnicodeError, KeyError):
            return False


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    @property
    def state(self):
        return self.server.state

    def respond(self, status, body=b"", kind="text/plain; charset=utf-8", challenge=None):
        if isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Referrer-Policy", "no-referrer")
        if challenge:
            self.send_header("WWW-Authenticate", challenge)
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def allowed(self):
        # This service has no configurable non-loopback binding or upstream URL.
        host = self.headers.get("Host", "")
        port = self.server.server_port
        return self.client_address[0] == "127.0.0.1" and host in (
            "127.0.0.1:" + str(port), "localhost:" + str(port))

    def endpoint(self):
        parsed = urlsplit(self.path)
        params = parse_qs(parsed.query)
        return parsed.path, params.get("run", [""])[0]

    def do_GET(self):
        path, run = self.endpoint()
        if not self.allowed() or run != self.state.run:
            self.respond(404, "Not found")
            return
        if path == "/":
            self.respond(200, self.index(), "text/html; charset=utf-8")
        elif path == "/auth":
            header = self.headers.get("Authorization", "")
            accepted = self.state.authenticate(header, "GET", self.path)
            self.state.record("auth-request", authorization_present=bool(header), accepted=accepted,
                              status=200 if accepted else 401)
            if accepted:
                self.respond(200, self.result_page("loaded", "Authenticated fixture response"),
                             "text/html; charset=utf-8")
            else:
                challenge = 'Basic realm="' + self.state.realm + '"'
                if self.state.scheme == "digest":
                    challenge = 'Digest realm="' + self.state.realm + '", nonce="' + self.state.nonce + '", algorithm=MD5, qop="auth"'
                # This body executes only if the browser delivers the final 401;
                # it does not tell the browser to stop or change authentication.
                self.respond(401, self.result_page("exhausted", "Authentication response is still 401"),
                             "text/html; charset=utf-8", challenge)
        elif path == "/landed":
            self.respond(200, self.result_page("navigated", "Navigation completed"), "text/html; charset=utf-8")
        elif path == "/control":
            self.respond(200, json.dumps({"command": self.state.command}), "application/json")
        elif path == "/state":
            self.respond(200, json.dumps({"run": run, "scheme": self.state.scheme,
                "case": self.state.case, "events": self.state.events}), "application/json")
        else:
            self.respond(404, "Not found")

    def do_POST(self):
        path, run = self.endpoint()
        origin = self.headers.get("Origin")
        if not self.allowed() or run != self.state.run or (origin and origin != "http://" + self.headers.get("Host", "")):
            self.respond(404, "Not found")
            return
        try:
            size = int(self.headers.get("Content-Length", "0"))
            if not 0 < size <= 256:
                raise ValueError()
            body = self.rfile.read(size).decode("ascii")
        except (ValueError, UnicodeError):
            self.respond(400, "Invalid bounded event")
            return
        if path == "/event" and body in EVENTS:
            self.state.record("page-" + body)
        elif path == "/command" and body in ("navigate", "ping"):
            if body == "navigate" and not self.state.case.startswith("navigate"):
                self.respond(400, "Not a navigation case")
                return
            self.state.command = body
            self.state.record("command-" + body)
        else:
            self.respond(400, "Unknown event")
            return
        self.respond(204)

    def result_page(self, event, title):
        return ('<!doctype html><meta charset="utf-8"><h2>' + title + '</h2><script>'
            'var x=new XMLHttpRequest();x.open("POST","/event?run=' + self.state.run + '",true);'
            'x.send("' + event + '");console.log("HTTP-AUTH ' + self.state.run + ' ' + event + '");</script>')

    def index(self):
        run = self.state.run
        return '''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font:18px sans-serif;margin:12px}button{font:20px sans-serif;width:100%;height:72px}iframe{width:100%;height:150px;border:1px solid #999}</style>
<h2>HTTP authentication fixture</h2><p id="info"></p><button id="start">Start challenge</button><iframe id="target"></iframe><p id="status">Ready</p>
<script>
var run=RUN, started=false;
document.getElementById('info').textContent=INFO;
function event(name){var x=new XMLHttpRequest();x.open('POST','/event?run='+run,true);x.send(name);console.log('HTTP-AUTH '+run+' '+name);}
document.getElementById('start').onclick=function(){if(started)return;started=true;this.disabled=true;event('started');document.getElementById('status').textContent='Challenge requested';document.getElementById('target').src='/auth?run='+run;};
function poll(){var x=new XMLHttpRequest();x.open('GET','/control?run='+run,true);x.onload=function(){var c=JSON.parse(x.responseText).command;if(c==='navigate'){location.href='/landed?run='+run;return;}if(c==='ping'){event('responsive');return;}setTimeout(poll,250);};x.onerror=function(){setTimeout(poll,500);};x.send();}
event('ready');poll();
</script>'''.replace("RUN", json.dumps(run)).replace("INFO", json.dumps(self.state.scheme + " / " + self.state.case + " / " + run))


class Server(HTTPServer):
    def handle_error(self, request, client_address):
        # Avoid raw exception/HTTP diagnostics that could carry input headers.
        self.state.record("connection-error")


def make_server(port, state):
    server = Server(("127.0.0.1", port), Handler)
    server.state = state
    return server


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8794)
    parser.add_argument("--run-token", required=True)
    parser.add_argument("--scheme", choices=("basic", "digest"), required=True)
    parser.add_argument("--case", choices=CASES, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False, mode=0o700)
    os.chmod(args.output, 0o700)
    password = "".join(secrets.choice("abcdefghijklmnopqrstuvwxyz0123456789") for _ in range(12))
    private_json(args.output / "credentials.private.json", {"username": "fixtureuser", "password": password,
        "wrong_password": ("a" if password[0] != "a" else "b") + password[1:]})
    state = State(args.run_token, args.scheme, args.case, "fixtureuser", password, args.output / "events.jsonl")
    server = make_server(args.port, state)
    url = "http://localhost:" + str(server.server_port) + "/?run=" + args.run_token
    ready = {"url": url, "control_url": url.replace("localhost", "127.0.0.1"),
             "run": args.run_token, "scheme": args.scheme, "case": args.case,
             "pid": os.getpid(), "port": server.server_port}
    private_json(args.output / "ready.json", ready)
    print(json.dumps(ready), flush=True)
    try:
        server.serve_forever(poll_interval=.2)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
