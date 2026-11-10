#!/usr/bin/env python3
"""Six local HTTP origins for the fixed IDB quota regression (no guest launcher)."""
import argparse
import http.server
import json
from pathlib import Path
import re
import threading
import urllib.parse

from contract import CASES, validate

EXPECTED = {phase: len(cases) for phase, cases in CASES.items()}
ROOT = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--base-port", type=int, default=8790)
    parser.add_argument("--reports", type=Path, required=True)
    args = parser.parse_args()
    args.reports.mkdir(parents=True, exist_ok=True)
    lock = threading.Lock()

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            path = urllib.parse.urlsplit(self.path).path
            name = {"/": "index.html", "/index.html": "index.html", "/page.js": "page.js"}.get(path)
            if not name:
                self.send_error(404); return
            content = (ROOT / name).read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/javascript; charset=utf-8" if name.endswith(".js") else "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(content)))
            self.end_headers(); self.wfile.write(content)

        def do_POST(self):
            parsed = urllib.parse.urlsplit(self.path)
            query = urllib.parse.parse_qs(parsed.query)
            try:
                run, phase = query["run"][0], query["phase"][0]
                length = int(self.headers.get("Content-Length", "0"))
                if parsed.path != "/report" or not re.fullmatch(r"[A-Za-z0-9_-]{8,64}", run) or phase not in EXPECTED or not 0 < length <= 65536:
                    raise ValueError("invalid report URL/length")
                report = json.loads(self.rfile.read(length))
                expected_origin = "http://" + self.headers.get("Host", "")
                validate(report, run, phase, expected_origin, require_success=False)
                target = args.reports / (run + "-" + phase + ".json")
                with lock:
                    with target.open("x", encoding="utf-8") as stream:
                        json.dump(report, stream, indent=2); stream.write("\n")
            except (KeyError, IndexError, ValueError, TypeError, OSError) as error:
                self.send_error(400, str(error)); return
            self.send_response(201); self.send_header("Content-Length", "0"); self.end_headers()

    servers = [http.server.ThreadingHTTPServer((args.bind, args.base_port + index), Handler) for index in range(6)]
    try:
        for server in servers:
            threading.Thread(target=server.serve_forever, daemon=True).start()
        print("READY ports=" + ",".join(str(args.base_port + index) for index in range(6)), flush=True)
        threading.Event().wait()
    except KeyboardInterrupt:
        pass
    finally:
        for server in servers:
            server.shutdown(); server.server_close()


if __name__ == "__main__":
    main()
