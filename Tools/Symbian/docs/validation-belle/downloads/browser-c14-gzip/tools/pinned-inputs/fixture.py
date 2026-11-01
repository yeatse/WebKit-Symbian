#!/usr/bin/env python3
"""One ordinary download case, reusing the repository's HTTP payload fixture."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import threading
from urllib.parse import parse_qs, urlsplit

CASES = {'post': '/post-attachment', 'get': '/attachment', 'gzip': '/gzip',
         'chunked': '/chunked', 'cancel-save': '/attachment',
         'cancel-transfer': '/slow', 'existing': '/attachment'}

def load_source(path):
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('ordinary_download_source', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def make_server(source, address, output, run, case, gate_seconds=180):
    class Server(source.FixtureServer):
        def __init__(self):
            super().__init__(address, output / 'events.jsonl', 0)
            self.sequence = 0
            self.release = threading.Event()
        def record(self, value):
            with self.log_lock:
                self.sequence += 1
                value = {**value, 'run': run, 'case': case, 'sequence': self.sequence,
                         'monotonic_ns': source.time.monotonic_ns()}
                with self.log_path.open('a', encoding='utf-8') as stream:
                    stream.write(json.dumps(value) + '\n')

    class Handler(source.Handler):
        def respond(self):
            parts = urlsplit(self.path)
            token = parse_qs(parts.query).get('run', [''])[0]
            if token != run:
                self.send_error(400, 'run mismatch')
                return
            if parts.path == '/release' and self.command == 'POST' and case == 'cancel-transfer':
                self.server.record({'event': 'release-requested'})
                self.server.release.set()
                self.send_response(200)
                self.send_header('Content-Length', '0')
                self.end_headers()
                return
            if parts.path == '/slow' and self.command == 'GET' and case == 'cancel-transfer':
                self.slow()
                return
            if parts.path not in ('/', CASES[case]):
                self.send_error(404)
                return
            super().respond()

        def slow(self):
            self.server.record({'event': 'request', 'method': 'GET', 'path': '/slow',
                                'body_bytes': 0, 'body_sha256': hashlib.sha256(b'').hexdigest(),
                                'authorization_present': 'Authorization' in self.headers,
                                'cookie_present': 'Cookie' in self.headers})
            self.send_response(200)
            self.send_header('Content-Type', 'application/octet-stream')
            self.send_header('Content-Disposition', 'attachment; filename="payload.bin"')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Transfer-Encoding', 'chunked')
            self.end_headers()
            written = 0
            try:
                for offset in range(0, len(source.PAYLOAD), 4096):
                    if offset == 8192:
                        self.server.record({'event': 'gate-wait', 'path': '/slow', 'wire_body_bytes': written})
                        if not self.server.release.wait(gate_seconds):
                            self.server.record({'event': 'gate-timeout', 'path': '/slow'})
                            self.close_connection = True
                            return
                    chunk = source.PAYLOAD[offset:offset + 4096]
                    self.wfile.write(f'{len(chunk):x}\r\n'.encode() + chunk + b'\r\n')
                    self.wfile.flush()
                    written += len(chunk)
                self.wfile.write(b'0\r\n\r\n')
                self.wfile.flush()
                self.server.record({'event': 'sent', 'path': '/slow', 'wire_body_bytes': written,
                                    'decoded_sha256': source.PAYLOAD_SHA256})
            except (BrokenPipeError, ConnectionResetError):
                self.close_connection = True
                self.server.record({'event': 'disconnected', 'path': '/slow', 'wire_body_bytes': written})

        def send_index(self, token):
            action = CASES[case] + '?run=' + token
            control = (f'<form method="post" action="{action}"><input type="hidden" name="token" value="post-once">'
                       '<button id="start">POST attachment once</button></form>' if case == 'post'
                       else f'<a id="start" href="{action}">Start {case}</a>')
            ready = json.dumps({'run': run, 'case': case}, separators=(',', ':'))
            body = ('<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
                    '<title>Download runtime</title><style>body{font:16px sans-serif;margin:16px}#start{display:block;padding:18px;'
                    'font:20px sans-serif;margin:18px 0;width:220px;box-sizing:border-box}p{overflow-wrap:anywhere}</style>'
                    f'<h2>Download: {case}</h2><p>Run: {run}</p>{control}'
                    '<p>Use the native save dialog. A click is not a PASS.</p>'
                    f'<script>console.log(\'READY: belle-download-runtime {ready}\');</script>').encode()
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(body)

    server = Server()
    server.RequestHandlerClass = Handler
    return server

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path('/workspace/WebKit-Symbian-graphics/Tools/Symbian/tests/downloads/server.py'))
    parser.add_argument('--port', type=int, default=8800)
    parser.add_argument('--run', required=True)
    parser.add_argument('--case', choices=CASES, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--gate-seconds', type=float, default=180)
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', args.run) or not 30 <= args.gate_seconds <= 300:
        parser.error('invalid run token or gate duration (30–300 seconds)')
    args.output.mkdir(parents=True, exist_ok=False)
    source = load_source(args.source)
    server = make_server(source, ('127.0.0.1', args.port), args.output, args.run, args.case, args.gate_seconds)
    ready = {'run': args.run, 'case': args.case,
             'url': f'http://localhost:{server.server_port}/?run={args.run}',
             'host_control_url': f'http://127.0.0.1:{server.server_port}',
             'payload_bytes': len(source.PAYLOAD), 'payload_sha256': source.PAYLOAD_SHA256,
             'fixture_source': str(args.source.resolve()),
             'fixture_source_sha256': hashlib.sha256(args.source.read_bytes()).hexdigest(),
             'scope': 'ordinary download behavior only', 'gate_seconds': args.gate_seconds}
    (args.output / 'ready.json').write_text(json.dumps(ready, indent=2) + '\n')
    print(json.dumps(ready), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.release.set()
        server.server_close()

if __name__ == '__main__':
    main()
