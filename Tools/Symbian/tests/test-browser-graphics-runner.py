#!/usr/bin/env python3
"""Host-only acceptance/cleanup regressions; never starts EKA or uses its XDG data."""
import argparse
import base64
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("graphics_runner", Path(__file__).with_name("run-browser-graphics-tests.py"))
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)
PNG = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+jX1sAAAAASUVORK5CYII=")
INITIALIZED = "Symbian compositor: native EGL/GLES2 initialized, renderer=fixture"
PRESENTED = "Symbian compositor: native EGL/GLES2 presented 360x640"


def guest(suite, url):
    prefix = "PASS: " if suite == "compositing" else "WEBGL PASS "
    messages = [prefix + name for name in RUNNER.EXPECTED[suite]]
    if suite == "compositing":
        messages.append("RESULT: compositing-regression passed=10 failed=0 frames=4")
    messages.append(RUNNER.MARKERS[suite])
    return INITIALIZED + "\n" + PRESENTED + "\n" + "\n".join("console: " + url + ":42: " + message for message in messages) + "\n"


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="graphics-runner-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.shot = self.root / "shot.png"
        self.shot.write_bytes(PNG)
        self.url = "file:///C:/Data/WebKit/compositing.html?autorun=1&run=new"

    def validate(self, text, host="normal fixture host", suite="compositing"):
        return RUNNER.validate_evidence(suite, self.url, text, host, self.shot)

    def test_both_complete_suites(self):
        for suite in RUNNER.EXPECTED:
            with self.subTest(suite=suite):
                result = self.validate(guest(suite, self.url), suite=suite)
                self.assertEqual(result["page_checks"], len(RUNNER.EXPECTED[suite]))

    def test_rejects_missing_or_conflicting_evidence(self):
        text = guest("compositing", self.url)
        bad = {
            "stale nonce": text.replace("run=new", "run=old"),
            "wrong count": text.replace("passed=10", "passed=9"),
            "wrong check": text.replace("PASS: layer opacity", "PASS: bogus"),
            "duplicate check": text + "console: " + self.url + ":42: PASS: layer opacity\n",
            "duplicate completion": text + "console: " + self.url + ":42: PASS: compositing-regression\n",
            "page failure": text + "console: " + self.url + ":42: FAIL: extra\n",
            "no initialization": text.replace(INITIALIZED, ""),
            "no presentation": text.replace(PRESENTED, ""),
            "invalid dimensions": text.replace("360x640", "0x640"),
            "software fallback": text + "Symbian compositor: GLES draw failed, using software painting\n",
        }
        for reason, value in bad.items():
            with self.subTest(reason=reason), self.assertRaises(RuntimeError):
                self.validate(value)

    def test_rejects_panic_and_missing_screenshot(self):
        with self.assertRaises(RuntimeError):
            self.validate(guest("compositing", self.url), "KERN-EXEC 3")
        self.shot.unlink()
        with self.assertRaises(RuntimeError):
            self.validate(guest("compositing", self.url))

    def test_webgl_failure_after_success(self):
        text = guest("webgl", self.url) + "console: " + self.url + ":42: WEBGL DONE FAIL 8/9 passed; 1 failed\n"
        with self.assertRaises(RuntimeError):
            self.validate(text, suite="webgl")


class DriverTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="graphics-driver-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.work = self.root / "work"
        (self.work / "runs").mkdir(parents=True)
        self.data = self.root / "data"
        self.data.mkdir()
        self.output = self.root / "output"
        self.output.mkdir()
        self.driver = self.root / "fake-driver.py"
        self.driver.write_text('''#!/usr/bin/env python3
import base64, json, os
from pathlib import Path
import sys, tempfile, time
fixture = json.loads(Path(os.environ['GRAPHICS_FIXTURE']).read_text())
mode = fixture['mode']
if mode == 'hang':
    time.sleep(30)
runs = Path(os.environ['SYMBIAN_WEBKIT_WORK']) / 'runs'
run = runs / 'browser-old' if mode == 'stale' else Path(tempfile.mkdtemp(prefix='browser-fixture-', dir=runs))
run.mkdir(exist_ok=True)
url = sys.argv[2]
(run / 'log.txt').write_text(fixture['guest'].replace('$URL$', url))
(run / 'emulator.log').write_text('normal fixture host\\n')
if mode != 'no-screenshot':
    (run / 'browser.png').write_bytes(base64.b64decode(fixture['png']))
print('logs: ' + str(run), flush=True)
sys.exit(7 if mode == 'nonzero' else 0)
''')
        self.driver.chmod(0o755)
        self.fixture = self.root / "fixture.json"
        self.args = argparse.Namespace(browser=self.driver, timeout=0.1, data=self.data)
        self.environment = patch.dict(os.environ, {"GRAPHICS_FIXTURE": str(self.fixture)})
        self.environment.start()
        self.addCleanup(self.environment.stop)

    def invoke(self, mode):
        self.fixture.write_text(json.dumps({"mode": mode, "guest": guest("compositing", "$URL$"), "png": base64.b64encode(PNG).decode()}))
        if mode == "stale":
            (self.work / "runs/browser-old").mkdir()
        evidence = {}
        with patch.object(RUNNER, "BROWSER_RUNNER", self.driver), patch.object(RUNNER, "DRIVER_GRACE", 0.1):
            with RUNNER.SUPPORT.exclusive_lock(self.data / ".browser-run.lock") as lock:
                RUNNER.run_suite(self.args, "compositing", self.output, self.work, lock, evidence)
        return evidence

    def test_success_copies_evidence(self):
        result = self.invoke("pass")
        self.assertEqual(result["status"], "PASS")
        self.assertTrue((self.output / "compositing/log.txt").is_file())
        self.assertEqual((self.output / "compositing/browser.png").read_bytes(), PNG)

    def test_driver_nonzero_cannot_pass(self):
        with self.assertRaisesRegex(RuntimeError, "exited 7"):
            self.invoke("nonzero")
        self.assertTrue((self.output / "compositing/log.txt").is_file())

    def test_watchdog_cannot_pass(self):
        with self.assertRaisesRegex(RuntimeError, "watchdog"):
            self.invoke("hang")

    def test_stale_run_cannot_pass(self):
        with self.assertRaisesRegex(RuntimeError, "stale"):
            self.invoke("stale")

    def test_missing_screenshot_cannot_pass(self):
        with self.assertRaisesRegex(RuntimeError, "PNG screenshot"):
            self.invoke("no-screenshot")

    def test_main_preserves_ca_and_records_hashes(self):
        self.fixture.write_text(json.dumps({"mode": "pass", "guest": guest("compositing", "$URL$"), "png": base64.b64encode(PNG).decode()}))
        ca = self.data / "drives/c/resource/webkit/cacert.pem"
        ca.parent.mkdir(parents=True)
        ca.write_bytes(b"host fixture original CA\n")
        for name in ("browser_reg.rsc", "browser_loc.rsc"):
            (self.driver.parent / name).write_bytes(b"fixture")
        target = self.root / "main-results"
        env = {"EKA2L1": str(self.driver), "DISPLAY": ":fixture", "SYMBIAN_WEBKIT_WORK": str(self.work)}
        with patch.dict(os.environ, env), patch.object(RUNNER, "BROWSER_RUNNER", self.driver):
            result = RUNNER.main([str(self.driver), "--suite", "compositing", "--data", str(self.data), "--output", str(target), "--timeout", "1"])
        self.assertEqual(result, 0)
        evidence = json.loads((target / "result.json").read_text())
        self.assertTrue(evidence["ca_unchanged"])
        self.assertEqual(evidence["browser_sha256"], RUNNER.sha256(self.driver))
        self.assertEqual(ca.read_bytes(), b"host fixture original CA\n")


if __name__ == "__main__":
    unittest.main()
