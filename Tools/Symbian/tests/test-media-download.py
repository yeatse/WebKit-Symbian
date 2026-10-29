#!/usr/bin/env python3
"""Build and exercise the real host helper; never starts EKA or changes guest trust.

Source host-build-env.sh first when libcurl lives in an isolated sysroot.
"""
import argparse
import contextlib
import http.server
import os
from pathlib import Path
import ssl
import subprocess
import sys
import tempfile
import threading
import time
import unittest


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        try:
            if self.path == "/missing":
                self.send_error(404)
            elif self.path in ("/redirect", "/loop", "/local"):
                self.send_response(302)
                self.send_header("Location", {"/redirect": "/ok", "/loop": "/loop", "/local": "file:///etc/hosts"}[self.path])
                self.send_header("Content-Length", "0")
                self.end_headers()
            elif self.path == "/slow":
                self.send_response(200)
                self.send_header("Content-Length", "100000")
                self.end_headers()
                for _ in range(100):
                    self.wfile.write(b"x" * 1000)
                    self.wfile.flush()
                    time.sleep(.03)
            else:
                self.send_response(200 if self.path == "/ok" else 206)
                self.send_header("Content-Length", str(len(b"native-media-test\n")))
                self.end_headers()
                self.wfile.write(b"native-media-test\n")
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            pass

    def log_message(self, *_):
        pass


@contextlib.contextmanager
def server(cert=None, key=None):
    class FixtureServer(http.server.ThreadingHTTPServer):
        def handle_error(self, request, client_address):
            # Certificate-negative clients deliberately close during TLS setup.
            if isinstance(sys.exception(), (BrokenPipeError, ConnectionResetError, ssl.SSLError)):
                return
            super().handle_error(request, client_address)
    instance = FixtureServer(("127.0.0.1", 0), Handler)
    if cert:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(cert, key)
        instance.socket = context.wrap_socket(instance.socket, server_side=True)
    thread = threading.Thread(target=instance.serve_forever, daemon=True)
    thread.start()
    try:
        yield ("https" if cert else "http") + f"://localhost:{instance.server_port}"
    finally:
        instance.shutdown()
        instance.server_close()
        thread.join()


class Downloads(unittest.TestCase):
    def probe(self, url, expected, mode=None):
        result = subprocess.run([str(BINARY), url, expected] + ([mode] if mode else []),
                                env=ENV, capture_output=True, text=True, timeout=12)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS: media-download", result.stdout)

    def test_success_and_redirect(self):
        self.probe(HTTP + "/ok", "complete")
        self.probe(HTTP.upper() + "/ok", "complete")
        self.probe(HTTP + "/redirect", "complete")

    def test_http_errors(self):
        self.probe(HTTP + "/missing", "http_error")
        self.probe(HTTP + "/partial", "http_error")

    def test_protocol_and_redirect_limits(self):
        self.probe("file:///etc/hosts", "invalid_url")
        self.probe(HTTP + "/ok", "invalid_url", "nul")
        self.probe(HTTP + "/local", "network_error")
        self.probe(HTTP + "/loop", "network_error")

    def test_cancel(self):
        self.probe(HTTP + "/slow", "cancelled", "cancel")

    def test_tls(self):
        self.probe(GOOD + "/ok", "complete")
        self.probe(WRONG + "/ok", "tls_error")
        self.probe(UNTRUSTED + "/ok", "tls_error")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("eka_source", type=Path)
    args = parser.parse_args()
    root = args.eka_source.resolve()
    here = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="eka-media-host-test-") as directory, contextlib.ExitStack() as stack:
        work = Path(directory)
        (work / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.20)
project(media_download_test LANGUAGES CXX)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
add_executable(probe "{here / 'media-download-host.cpp'}" "{root / 'src/emu/drivers/src/media_download.cpp'}")
target_include_directories(probe PRIVATE "{root / 'src/emu/drivers/include'}")
target_compile_features(probe PRIVATE cxx_std_17)
target_link_libraries(probe PRIVATE CURL::libcurl Threads::Threads)
''')
        subprocess.run(["cmake", "-S", str(work), "-B", str(work / "build"), "-G", "Ninja"], check=True)
        subprocess.run(["cmake", "--build", str(work / "build"), "--parallel", "1"], check=True)
        BINARY = work / "build/probe"
        certs = work / "certs"
        subprocess.run(["bash", str(here / "make-test-certificates.sh"), str(certs)], check=True)
        ENV = dict(os.environ, EKA2L1_MEDIA_CA_BUNDLE=str(certs / "ca.crt"), TMPDIR=str(work))
        HTTP = stack.enter_context(server())
        GOOD = stack.enter_context(server(certs / "good.crt", certs / "good.key"))
        WRONG = stack.enter_context(server(certs / "wrong.crt", certs / "wrong.key"))
        UNTRUSTED = stack.enter_context(server(certs / "untrusted.crt", certs / "untrusted.key"))
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Downloads))
        if list(work.glob("eka-media-*")):
            raise RuntimeError("helper leaked temporary media files")
        raise SystemExit(not result.wasSuccessful())
