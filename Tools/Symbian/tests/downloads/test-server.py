#!/usr/bin/env python3
import gzip
import http.client
import importlib.util
import json
from pathlib import Path
import tempfile
import threading
import unittest

spec = importlib.util.spec_from_file_location("download_fixture", Path(__file__).with_name("server.py"))
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class ServerTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.log = Path(self.directory.name) / "requests.jsonl"
        self.server = fixture.FixtureServer(("127.0.0.1", 0), self.log, 8801)
        self.thread = threading.Thread(target=self.server.serve_forever)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.directory.cleanup()

    def request(self, path, method="GET", body=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=5)
        connection.request(method, path, body=body)
        return connection, connection.getresponse()

    def test_payloads(self):
        for path in ("attachment", "attribute", "chunked", "unicode", "unsafe-name", "gzip"):
            with self.subTest(path=path):
                connection, response = self.request(f"/{path}?run=payload")
                self.assertEqual(response.status, 200)
                payload = response.read()
                if path == "gzip":
                    payload = gzip.decompress(payload)
                self.assertEqual(payload, fixture.PAYLOAD)
                connection.close()

    def test_post_is_logged_once(self):
        connection, response = self.request("/post-attachment?run=post", "POST", b"token=post-once")
        self.assertEqual(response.read(), fixture.PAYLOAD)
        connection.close()
        requests = [json.loads(line) for line in self.log.read_text().splitlines() if json.loads(line)["event"] == "request"]
        self.assertEqual(len(requests), 1)
        self.assertEqual(requests[0]["method"], "POST")
        self.assertEqual(requests[0]["body_bytes"], 15)

    def test_truncated_body(self):
        connection, response = self.request("/truncated?run=truncated")
        with self.assertRaises(http.client.IncompleteRead):
            response.read()
        connection.close()

    def test_rejection_fixtures(self):
        for path, status in (("not-found", 404), ("partial", 206), ("too-large", 200)):
            connection, response = self.request(f"/{path}?run=reject")
            self.assertEqual(response.status, status)
            if path == "too-large":
                self.assertEqual(response.getheader("Content-Length"), "2147483648")
            else:
                response.read()
            connection.close()

    def test_redirect_contract(self):
        for path, expected in (("redirect", "/attachment?run=redirect"),
                               ("redirect-other-port", "http://localhost:8801/attachment?run=redirect"),
                               ("redirect-file", "file:///C:/Data/private-download-test.bin"),
                               ("redirect-loop", "/redirect-loop?run=redirect")):
            connection, response = self.request(f"/{path}?run=redirect")
            self.assertEqual(response.status, 302)
            self.assertEqual(response.getheader("Location"), expected)
            self.assertEqual(response.read(), b"")
            connection.close()

    def test_index_separates_conversion_and_attribute(self):
        connection, response = self.request("/?run=index")
        text = response.read().decode()
        self.assertIn('href="/attachment?run=index">', text)
        self.assertIn('href="/attribute?run=index" download=', text)
        self.assertIn('method="post" action="/post-attachment?run=index"', text)
        connection.close()


if __name__ == "__main__":
    unittest.main()
