#!/usr/bin/env python3
"""Bounded host-only checks of EC browser evidence verification."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("ec_verify", Path(__file__).with_name("verify.py"))
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)

class VerifyTests(unittest.TestCase):
    def setUp(self):
        self.token = "ec-fixture-123"
        self.vectors = {"ecdsa": [{"name": "P-256 SHA-256"}], "ecdh": [{"curve": "P-256"}]}
        names = verifier.expected_names(self.vectors)
        self.report = {"run": self.token, "passed": len(names), "total": len(names), "elapsed_ms": 10,
                       "results": [{"name": name, "pass": True, "error": None} for name in names]}

    def log(self, report=None):
        report = report or self.report
        rows = [f"EC_CASE run={report['run']} PASS {row['name']}" for row in report["results"]]
        rows += ["EC_RESULT " + json.dumps(report), f"EC_DONE run={report['run']} passed={report['passed']} total={report['total']}"]
        return "\n".join(rows)

    def verify(self, log):
        return verifier.verify(log, self.token, self.vectors)

    def test_complete_report(self):
        self.assertEqual(self.verify(self.log())["passed"], len(self.report["results"]))

    def test_stale_identity(self):
        with self.assertRaises(ValueError): self.verify(self.log().replace(self.token, "stale-ec-run"))

    def test_duplicate_report(self):
        with self.assertRaises(ValueError): self.verify(self.log() + "\n" + self.log())

    def test_missing_marker(self):
        with self.assertRaises(ValueError): self.verify("\n".join(self.log().splitlines()[1:]))

    def test_reordered_cases(self):
        report = copy.deepcopy(self.report)
        report["results"][:2] = reversed(report["results"][:2])
        with self.assertRaises(ValueError): self.verify(self.log(report))

    def test_failed_case(self):
        report = copy.deepcopy(self.report)
        report["results"][0]["pass"] = False
        report["results"][0]["error"] = "fixture failure"
        with self.assertRaises(ValueError): self.verify(self.log(report))

    def test_deadline_and_invalid_elapsed(self):
        for value in (-1, 180001, True, "12"):
            report = copy.deepcopy(self.report); report["elapsed_ms"] = value
            with self.subTest(value=value), self.assertRaises(ValueError): self.verify(self.log(report))

    def test_timeout_and_native_failure(self):
        for text in ("EC_TIMEOUT", "EC_FATAL", "KERN-EXEC", "panicked"):
            with self.subTest(text=text), self.assertRaises(ValueError): self.verify(self.log() + "\n" + text)

if __name__ == "__main__": unittest.main()
