#!/usr/bin/env python3
"""Test the real runner with a local TLS fixture and synthetic browser driver.

No emulator, real XDG data or SDK is touched. Synthetic reports/screenshots only
exercise orchestration; they are never evidence of browser SameSite support.
"""

import importlib.util
import json
import os
import shutil
import signal
import socket
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.parse import parse_qs, urlsplit

HERE = Path(__file__).resolve().parent


def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, HERE / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


runner = load("cookie_runner_under_test", "run.py")
host_tests = load("cookie_synthetic_report_helpers", "test-host.py")

DRIVER = r"""#!/usr/bin/env python3
import base64, json, os, pathlib, sys, time
from urllib.parse import parse_qs, urlencode, urlsplit
entry = urlsplit(sys.argv[2])
token = parse_qs(entry.query)['run'][0]
phase = 'initial' if entry.path == '/start' else 'resumed'
fixtures = pathlib.Path(os.environ['FAKE_COOKIE_FIXTURES'])
report = json.loads((fixtures / (phase + '.json')).read_text())
report['config']['run'] = token
for index, request in enumerate(report['requests']):
    request['query']['run'] = token
    request['cookie'] = request['cookie'].replace('ss_synthetic_', 'ss_' + token + '_')
    request['time_ns'] = time.time_ns() + index
pathlib.Path(os.environ['FAKE_COOKIE_REPORTS'], token + '.json').write_text(json.dumps(report))
ca = pathlib.Path(os.environ['EKA2L1_DATA']) / 'drives/c/resource/webkit/cacert.pem'
with (fixtures / 'launches.jsonl').open('a') as stream:
    stream.write(json.dumps({'pid': os.getpid(), 'url': sys.argv[2], 'phase': phase,
                             'test_ca_present': b'BEGIN CERTIFICATE' in ca.read_bytes()}) + '\n')
run = pathlib.Path(os.environ['SYMBIAN_WEBKIT_WORK']) / 'runs' / ('browser-synthetic-' + phase + '-' + str(os.getpid()))
run.mkdir()
count = 23 if phase == 'initial' else 28
marker = 'PASS: cookie-samesite %d/%d (%d outcomes)' % (count, count, count)
query = {'run': token, 'phase': 'nav' if phase == 'initial' else 'resume-b'}
mode = os.environ.get('FAKE_COOKIE_MODE', '')
if mode == 'wrong-final-url': del query['phase']
final = entry.scheme + '://' + entry.netloc + '/done?' + urlencode(query)
source = entry.scheme + '://' + entry.netloc + '/case.js:999: '
if mode == 'wrong-script-url': source = entry.scheme + '://' + entry.netloc + '/other.js:999: '
line = 'console: ' + source + marker + '\n'
final_line = 'console: ' + source + 'COOKIE_FINAL_URL: ' + final + '\n'
if mode == 'missing-final-url': final_line = ''
(run / 'log.txt').write_text(final_line + line * (2 if mode == 'duplicate-marker' else 1))
(run / 'emulator.log').write_text('Synthetic host log for orchestration unit test; exit 0\n')
if mode != 'missing-png':
    (run / 'browser.png').write_bytes(base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLbtAAAAABJRU5ErkJggg=='))
print('logs: ' + str(run), flush=True)
raise SystemExit(7 if mode == 'nonzero-driver' else 0)
"""


class RunnerContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which("openssl"):
            raise unittest.SkipTest("openssl is required for a real local TLS fixture")
        cls.shared = tempfile.TemporaryDirectory(prefix="cookie runner certs ")
        cls.tls = Path(cls.shared.name) / "tls"
        subprocess.run(
            ["bash", str(HERE / "make-certificates.sh"), str(cls.tls)],
            check=True,
            capture_output=True,
            timeout=30,
        )

    @classmethod
    def tearDownClass(cls):
        cls.shared.cleanup()

    def perform(self, directory, mode=""):
        root = Path(directory)
        data, work, reports, fixtures = [
            root / name for name in ("data", "work", "reports", "fixtures")
        ]
        for path in (data, work, reports, fixtures):
            path.mkdir()
        ca = data / "drives/c/resource/webkit/cacert.pem"
        ca.parent.mkdir(parents=True)
        original = b"Original private test trust file; preserve exact bytes.\n"
        ca.write_bytes(original)
        ca.chmod(0o640)
        browser, host, driver = [
            root / name for name in ("browser.exe", "host.exe", "driver.py")
        ]
        browser.write_bytes(b"synthetic browser identity")
        host.write_bytes(b"never executed synthetic host identity")
        driver.write_text(DRIVER)
        driver.chmod(0o755)
        # Reserve distinct ephemeral ports together; no fixed runtime ports/XDG.
        sockets = [socket.socket() for _ in range(3)]
        for stream in sockets:
            stream.bind(("127.0.0.1", 0))
        ports = [stream.getsockname()[1] for stream in sockets]
        for stream in sockets:
            stream.close()
        for phase in ("initial", "resumed"):
            report = host_tests.ReportValidation().synthetic(phase == "resumed")
            content = json.dumps(report)
            for before, after in zip((8791, 8792, 8790), ports, strict=True):
                content = content.replace(":" + str(before), ":" + str(after))
            (fixtures / (phase + ".json")).write_text(content)
        output = root / "output"
        arguments = [
            "run.py",
            str(browser),
            "--ca",
            str(self.tls / "ca.crt"),
            "--tls",
            str(self.tls),
            "--reports",
            str(reports),
            "--output",
            str(output),
            "--data",
            str(data),
            "--origin",
            "https://localhost:" + str(ports[0]),
            "--other-port",
            str(ports[1]),
            "--http-port",
            str(ports[2]),
            "--timeout",
            "5",
            "--resume-timeout",
            "5",
        ]
        environment = {
            "DISPLAY": "synthetic-no-display",
            "EKA2L1": str(host),
            "EKA2L1_DATA": str(data),
            "XDG_DATA_HOME": str(root / "never-real-xdg"),
            "SYMBIAN_WEBKIT_WORK": str(work),
            "FAKE_COOKIE_FIXTURES": str(fixtures),
            "FAKE_COOKIE_REPORTS": str(reports),
            "FAKE_COOKIE_MODE": mode,
        }
        old_signals = {
            kind: signal.getsignal(kind) for kind in (signal.SIGTERM, signal.SIGINT)
        }
        try:
            with (
                patch.dict(os.environ, environment),
                patch("sys.argv", arguments),
                patch.object(runner.network, "BROWSER_RUNNER", driver),
            ):
                result = runner.main()
        finally:
            for kind, handler in old_signals.items():
                signal.signal(kind, handler)
        self.assertEqual(ca.read_bytes(), original)
        self.assertEqual(stat.S_IMODE(ca.stat().st_mode), 0o640)
        evidence = json.loads((output / "result.json").read_text())
        self.assertTrue(evidence["test_ca_restored"])
        self.assertEqual(
            evidence["release_ca_before_sha256"], evidence["release_ca_after_sha256"]
        )
        launches = [
            json.loads(line)
            for line in (fixtures / "launches.jsonl").read_text().splitlines()
        ]
        self.assertTrue(all(row["test_ca_present"] for row in launches))
        return result, evidence, launches

    def test_two_real_driver_processes_same_token_and_exact_final_urls(self):
        with tempfile.TemporaryDirectory(prefix="cookie runner good ") as directory:
            result, evidence, launches = self.perform(directory)
        self.assertFalse(result)
        self.assertEqual(evidence["status"], "PASS")
        self.assertEqual([row["phase"] for row in launches], ["initial", "resumed"])
        self.assertEqual(len({row["pid"] for row in launches}), 2)
        urls = [urlsplit(row["url"]) for row in launches]
        self.assertEqual([url.path for url in urls], ["/start", "/resume"])
        self.assertEqual(
            {parse_qs(url.query)["run"][0] for url in urls}, {evidence["run"]}
        )
        self.assertEqual(
            [row["cumulative_checks"] for row in evidence["phases"]], [23, 28]
        )

    def test_failure_artifacts_never_promoted_and_ca_always_restored(self):
        cases = {
            "nonzero-driver": "driver failed",
            "duplicate-marker": "unique final-page",
            "missing-png": "screenshot",
            "wrong-final-url": "unique final-page",
            "wrong-script-url": "unique final-page",
            "missing-final-url": "unique final-page",
        }
        for mode, expected in cases.items():
            with (
                self.subTest(mode=mode),
                tempfile.TemporaryDirectory(prefix="cookie runner bad ") as directory,
            ):
                result, evidence, launches = self.perform(directory, mode)
                self.assertTrue(result)
                self.assertEqual(evidence["status"], "FAIL")
                self.assertIn(expected, evidence["error"])
                self.assertEqual(len(launches), 1)


if __name__ == "__main__":
    unittest.main()
