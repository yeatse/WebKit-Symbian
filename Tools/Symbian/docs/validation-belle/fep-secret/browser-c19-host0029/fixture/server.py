#!/usr/bin/env python3
"""Only serve the public secret-FEP fixture, on loopback. Never starts itself."""
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
import argparse
ROOT=Path(__file__).resolve().parent
class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.headers.get('Host') not in {'localhost:'+str(self.server.server_port),'127.0.0.1:'+str(self.server.server_port)}:
            self.send_error(400);return
        if urlsplit(self.path).path not in ('/','/index.html'):
            self.send_error(404);return
        body=(ROOT/'index.html').read_bytes();self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Content-Length',str(len(body)));self.send_header('Cache-Control','no-store');self.end_headers();self.wfile.write(body)
def main():
    p=argparse.ArgumentParser();p.add_argument('--port',type=int,default=8813);a=p.parse_args()
    if a.port!=8813:p.error('dedicated fixture port is8813')
    ThreadingHTTPServer(('127.0.0.1',a.port),Handler).serve_forever()
if __name__=='__main__':main()
