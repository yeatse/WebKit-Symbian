#!/usr/bin/env python3
"""Regression checks for rejecting misleading or stale page evidence."""
import copy
import unittest
from contract import BACKEND_CASES, CASES, validate


class ResultContract(unittest.TestCase):
    def report(self, phase="write"):
        return {"suite": "belle-storage", "run": "fresh", "phase": phase,
                "origin": "http://localhost:8780", "status": "PASS", "elapsed_ms": 1200,
                "cases": [{"name": name, "status": "PASS"} for name in CASES[phase]]}

    def check(self, report, phase="write"):
        validate(report, "fresh", phase, "http://localhost:8780")

    def test_complete_three_phases(self):
        for phase in CASES:
            with self.subTest(phase=phase): self.check(self.report(phase), phase)

    def test_stale_or_different_origin(self):
        for field, value in (("run", "old"), ("origin", "http://localhost:8781"),
                             ("phase", "read"), ("suite", "other")):
            report = self.report(); report[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError): self.check(report)

    def test_missing_extra_and_duplicate_case(self):
        original = self.report()
        cases = original["cases"]
        for rows in (cases[:-1], cases + [cases[0]], cases[:-1] + [cases[0]], list(reversed(cases))):
            report = copy.deepcopy(original); report["cases"] = rows
            with self.subTest(rows=len(rows)), self.assertRaises(ValueError): self.check(report)

    def test_pass_summary_cannot_hide_failure(self):
        report = self.report(); report["cases"][3]["status"] = "FAIL"
        with self.assertRaises(ValueError): self.check(report)

    def test_all_cases_cannot_hide_failed_summary(self):
        report = self.report(); report["status"] = "FAIL"
        with self.assertRaises(ValueError): self.check(report)

    def test_deadline_and_invalid_types(self):
        for elapsed in (-1, 45001, "1200", True, float("nan")):
            report = self.report(); report["elapsed_ms"] = elapsed
            with self.subTest(elapsed=elapsed), self.assertRaises(ValueError): self.check(report)

    def test_backend_phases(self):
        for phase, names in BACKEND_CASES.items():
            report = self.report(phase)
            report.update(suite="belle-backends", origin="http://localhost:8784",
                          cases=[{"name": name, "status": "PASS"} for name in names])
            with self.subTest(phase=phase):
                validate(report, "fresh", phase, "http://localhost:8784", "backends")

    def test_backend_cannot_satisfy_storage_contract(self):
        report = self.report(); report["suite"] = "belle-backends"
        with self.assertRaises(ValueError): self.check(report)


if __name__ == "__main__":
    unittest.main()
