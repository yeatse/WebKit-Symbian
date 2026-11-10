#!/usr/bin/env python3
import copy
import json
import unittest
from verify import EXTRA, verify

class Contract(unittest.TestCase):
    def setUp(self):
        self.token = "rsa-test-token"
        self.vectors = [{"name": "known vector"}]
        self.names = ["known answer known vector"] + EXTRA
        self.report = {"run": self.token, "total": len(self.names), "passed": len(self.names), "elapsed_ms": 5, "results": [{"name": name, "pass": True, "error": None} for name in self.names]}
    def log(self, report=None):
        return "\n".join(["RSA_CASE run=" + self.token + " PASS " + name for name in self.names] + ["RSA_RESULT " + json.dumps(report or self.report), f"RSA_DONE run={self.token} passed={len(self.names)} total={len(self.names)}"])
    def test_complete(self): self.assertEqual(verify(self.log(), self.token, self.vectors), self.report)
    def test_stale(self):
        with self.assertRaises(ValueError): verify(self.log(), "other-token", self.vectors)
    def test_missing(self):
        value = copy.deepcopy(self.report); value["results"].pop()
        with self.assertRaises(ValueError): verify(self.log(value), self.token, self.vectors)
    def test_nonboolean(self):
        value = copy.deepcopy(self.report); value["results"][0]["pass"] = 1
        with self.assertRaises(ValueError): verify(self.log(value), self.token, self.vectors)
    def test_duplicate(self):
        with self.assertRaises(ValueError): verify(self.log() + "\nRSA_RESULT " + json.dumps(self.report), self.token, self.vectors)
    def test_timeout(self):
        value = copy.deepcopy(self.report); value["elapsed_ms"] = 180001
        with self.assertRaises(ValueError): verify(self.log(value), self.token, self.vectors)
    def test_missing_console(self):
        with self.assertRaises(ValueError): verify(self.log().replace("RSA_CASE", "REMOVED", 1), self.token, self.vectors)

if __name__ == "__main__": unittest.main()
