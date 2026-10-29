#!/usr/bin/env python3
"""Host fixtures for EKA tier evidence and process-exit validation; no guest runs."""

import fcntl
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import time
import unittest

from arm11_jit_common import CASES

TOOLS = Path(__file__).resolve().parents[1]


def fixture_output(mode):
    result = {"mode": mode, "completed": list(CASES), "digest": list(range(len(CASES))),
              "tiers": [True, mode != "llint", mode == "dfg"],
              "compiled": {name: int(mode == "dfg") for name in CASES[1:]},
              "dfgBodies": {name: mode == "dfg" for name in CASES[2:]},
              "osrEntry": {"startedInDFG": False, "enteredDFG": mode == "dfg"},
              "osrExit": {"before": mode == "dfg", "after": False}}
    failure = os.environ.get("ARM11_FIXTURE", "")
    if failure == "missing-tier":
        result["tiers"] = [True, False, False]
    if failure == "different-result" and mode == "dfg":
        result["digest"][0] += 1
    return "\n".join([*("PASS: " + name for name in CASES), "RESULT " + json.dumps(result),
                      "PASS: arm11-jit 10/10", "jsc exiting 0", ""])


def fake_emulator():
    mode = shlex.split(sys.argv[-1])[-1]
    failure = os.environ.get("ARM11_FIXTURE", "")
    if failure != "stale-only":
        guest = Path(os.environ["XDG_DATA_HOME"]) / "EKA2L1/data/drives/c/Data/jsc.log"
        guest.write_text(fixture_output(mode))
    if failure == "host-panic":
        print("Emulated thread panicked KERN-EXEC 3", flush=True)
    if failure == "timeout":
        time.sleep(60)
    return 7 if failure == "exit-seven" else 0


class RunnerTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.data = self.root / "xdg/EKA2L1/data"
        self.data.mkdir(parents=True)
        self.image = self.root / "jsc.exe"
        self.image.write_bytes(b"host test fixture, not an E32 image")
        self.icu = self.root / "icudt70l.dat"
        self.icu.write_bytes(b"host test fixture")
        self.emulator = self.root / "fake-emulator"
        self.emulator.write_text("#!/bin/sh\nexec " + shlex.quote(sys.executable) + " "
                                 + shlex.quote(str(Path(__file__).resolve())) + " --fake \"$@\"\n")
        self.emulator.chmod(0o755)

    def run_fixture(self, failure="", modes=("llint", "baseline", "dfg")):
        command = [sys.executable, str(TOOLS / "eka2l1/run-jit-tests.py"),
                   "--image", str(self.image), "--icu-data", str(self.icu),
                   "--emulator", str(self.emulator), "--data", str(self.data),
                   "--reports", str(self.root / "reports"), "--timeout", "0.25" if failure == "timeout" else "5",
                   "--modes", *modes]
        result = subprocess.run(command, env=dict(os.environ, ARM11_FIXTURE=failure),
                                text=True, capture_output=True, timeout=15)
        reports = list((self.root / "reports").glob("*/result.json"))
        return result, json.loads(reports[0].read_text()) if reports else None

    def test_all_tiers_and_matching_results(self):
        process, report = self.run_fixture()
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual([item["mode"] for item in report["profiles"]], ["llint", "baseline", "dfg"])
        self.assertEqual(report["image_sha256"], report["installed_image_sha256"])

    def test_markers_do_not_replace_tier_evidence(self):
        process, report = self.run_fixture("missing-tier", ("baseline",))
        self.assertNotEqual(process.returncode, 0)
        self.assertIn("never ran", report["failure"])

    def test_pass_cannot_hide_host_failure(self):
        process, report = self.run_fixture("exit-seven", ("llint",))
        self.assertNotEqual(process.returncode, 0)
        self.assertEqual(report["profiles"][0]["host_exit"], 7)

    def test_pass_cannot_hide_timeout(self):
        process, report = self.run_fixture("timeout", ("llint",))
        self.assertNotEqual(process.returncode, 0)
        self.assertEqual(report["failure"], "host timeout")

    def test_stale_guest_output_cannot_pass(self):
        guest = self.data / "drives/c/Data/jsc.log"
        guest.parent.mkdir(parents=True)
        guest.write_text(fixture_output("llint"))
        process, report = self.run_fixture("stale-only", ("llint",))
        self.assertNotEqual(process.returncode, 0)
        self.assertIn("missing successful guest exit", report["failure"])

    def test_host_panic_cannot_pass(self):
        process, report = self.run_fixture("host-panic", ("llint",))
        self.assertNotEqual(process.returncode, 0)
        self.assertIn("panic", report["failure"])

    def test_cross_tier_semantics_must_match(self):
        process, report = self.run_fixture("different-result")
        self.assertNotEqual(process.returncode, 0)
        self.assertIn("differ", report["failure"])

    def test_emulator_lock_prevents_run(self):
        with (self.data / ".jsc-run.lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            process, report = self.run_fixture()
        self.assertNotEqual(process.returncode, 0)
        self.assertIsNone(report)
        self.assertFalse((self.data / "drives/c/sys/bin/jsc_arm11.exe").exists())


if __name__ == "__main__":
    if "--fake" in sys.argv:
        sys.exit(fake_emulator())
    unittest.main()
