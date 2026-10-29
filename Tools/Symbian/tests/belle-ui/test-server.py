#!/usr/bin/env python3
"""Host HTTP checks only; these do not validate the native dialogs."""

import hashlib
import json
from pathlib import Path
import tempfile
import threading
import unittest
import urllib.error
import urllib.request

import server


class QuietHandler(server.Handler):
    def log_message(self, *args):
        pass


class UploadTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.http = server.ThreadingHTTPServer(("127.0.0.1", 0), QuietHandler)
        cls.http.evidence = Path(cls.directory.name) / "uploads.jsonl"
        cls.thread = threading.Thread(target=cls.http.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = "http://127.0.0.1:" + str(cls.http.server_port)

    @classmethod
    def tearDownClass(cls):
        cls.http.shutdown()
        cls.http.server_close()
        cls.thread.join()
        cls.directory.cleanup()

    def upload(self, payload=server.SAMPLE, *, field="file", origin=None, close=True):
        boundary = "belle-ui-host-test"
        body = ('--' + boundary + '\r\nContent-Disposition: form-data; name="' + field
                + '"; filename="' + server.SAMPLE_NAME + '"\r\nContent-Type: text/plain\r\n\r\n').encode() + payload
        if close:
            body += b"\r\n--" + boundary.encode() + b"--\r\n"
        request = urllib.request.Request(self.base + "/upload", data=body, headers={
            "Content-Type": "multipart/form-data; boundary=" + boundary,
            "Origin": origin or self.base})
        with urllib.request.urlopen(request) as response:
            return json.load(response)

    def test_unicode_real_http(self):
        result = self.upload()
        self.assertEqual(result["name"], server.SAMPLE_NAME)
        self.assertEqual(result["bytes"], len(server.SAMPLE))
        self.assertEqual(result["sha256"], hashlib.sha256(server.SAMPLE).hexdigest())
        self.assertTrue(result["contentMatches"])

    def test_content_mismatch(self):
        self.assertFalse(self.upload(b"wrong bytes")["contentMatches"])

    def assert_bad_request(self, **kwargs):
        with self.assertRaises(urllib.error.HTTPError) as error:
            self.upload(**kwargs)
        self.assertEqual(error.exception.code, 400)

    def test_wrong_field(self):
        self.assert_bad_request(field="unrelated")

    def test_cross_origin(self):
        self.assert_bad_request(origin="http://other.invalid")

    def test_truncated_multipart(self):
        self.assert_bad_request(close=False)

    def test_body_limit(self):
        self.assert_bad_request(payload=b"x" * server.MAX_BODY)

    def test_fixture_served(self):
        with urllib.request.urlopen(self.base + "/") as response:
            self.assertIn(b"Belle native UI regression", response.read())


if __name__ == "__main__":
    unittest.main()
