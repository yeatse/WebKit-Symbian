#!/usr/bin/env python3
"""Serve the clipboard fixture and preserve one immutable report per run/mode."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
from urllib.parse import urlsplit
from contract import identity, validate_shape

ROOT = Path(__file__).resolve().parent
FILES = {'/': ('index.html', 'text/html'), '/index.html': ('index.html', 'text/html'), '/suite.js': ('suite.js', 'text/javascript')}


class Handler(BaseHTTPRequestHandler):
    def reply(self, status, data, mime='application/json'):
        self.send_response(status)
        self.send_header('Content-Type', mime + '; charset=utf-8')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        item = FILES.get(urlsplit(self.path).path)
        if not item:
            self.reply(404, b'not found', 'text/plain'); return
        name, mime = item
        self.reply(200, (ROOT / name).read_bytes(), mime)

    def do_POST(self):
        if urlsplit(self.path).path != '/report':
            self.reply(404, b'not found', 'text/plain'); return
        try:
            origin = 'http://' + self.headers.get('Host', '')
            if self.headers.get('Origin') != origin:
                raise ValueError('same-origin report required')
            length = int(self.headers.get('Content-Length', '0'))
            if not 0 < length <= 131072:
                raise ValueError('invalid report length')
            self.connection.settimeout(10)
            body = self.rfile.read(length)
            if len(body) != length:
                raise ValueError('truncated report')
            report = json.loads(body)
            validate_shape(report)
            if report['origin'] != origin:
                raise ValueError('report origin differs from request')
            run, mode = identity(report)
            with (self.server.reports / (run + '-' + mode + '.json')).open('xb') as stream:
                stream.write(body + b'\n')
        except FileExistsError:
            self.reply(409, b'report already exists', 'text/plain'); return
        except (OSError, ValueError, TypeError) as error:
            self.reply(400, str(error).encode(), 'text/plain'); return
        self.reply(201, b'{"stored":true}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bind', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=8802)
    parser.add_argument('--reports', type=Path, required=True)
    args = parser.parse_args()
    args.reports.mkdir(parents=True, exist_ok=True)
    server = ThreadingHTTPServer((args.bind, args.port), Handler)
    server.reports = args.reports
    print('READY', server.server_address, flush=True)
    try: server.serve_forever()
    finally: server.server_close()


if __name__ == '__main__':
    main()
