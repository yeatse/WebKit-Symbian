#!/usr/bin/env python3
import copy
import unittest
from contract import CASES, validate

class ContractTests(unittest.TestCase):
    def report(self, phase="write"):
        return {"schema": 1, "run": "contract-token", "phase": phase, "origin": "http://localhost:8790", "elapsed_ms": 1,
                "passed": len(CASES[phase]), "total": len(CASES[phase]),
                "results": [{"name": name, "pass": True, "details": None, "error": None} for name in CASES[phase]]}
    def rejects(self, edit):
        report = self.report(); edit(report)
        with self.assertRaises(ValueError): validate(report, "contract-token", "write", "http://localhost:8790")
    def test_complete(self):
        for phase in CASES: validate(self.report(phase), "contract-token", phase, "http://localhost:8790")
    def test_missing_case(self): self.rejects(lambda r: r["results"].pop())
    def test_wrong_token(self): self.rejects(lambda r: r.update(run="previous-token"))
    def test_wrong_origin(self): self.rejects(lambda r: r.update(origin="http://localhost:8791"))
    def test_bad_total(self): self.rejects(lambda r: r.update(passed=0))
    def test_reordered(self): self.rejects(lambda r: r["results"].reverse())
    def test_timeout(self): self.rejects(lambda r: r.update(elapsed_ms=240001))
    def test_false_result(self): self.rejects(lambda r: (r["results"][0].update({"pass": False}), r.update(passed=r["total"]-1)))
    def test_false_boolean(self): self.rejects(lambda r: r["results"][0].update({"pass": 1}))
    def test_failure_can_be_archived(self):
        report = self.report(); report["results"][0]["pass"] = False; report["passed"] -= 1
        validate(report, "contract-token", "write", "http://localhost:8790", require_success=False)

if __name__ == "__main__": unittest.main()
