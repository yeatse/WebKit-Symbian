#!/usr/bin/env python3
"""Host-only regression fixtures; no emulator or GUI access."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("exit_validator", Path(__file__).with_name("validate.py"))
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


class ExitValidatorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name)
        self.identities = {name + "_sha256": str(index) * 64
                           for index, name in enumerate(("browser", "emulator", "helper"), 1)}
        self.record = dict(self.identities, wm_close_sent=False, forced_cleanup=False,
                           url="http://localhost:8782/diagnostics.html?case=exit&run=fresh",
                           host_exit_code=0, finished_utc="2026-10-29T20:00:00Z")
        for name in ("launch.json", "observation.json"):
            self.write_record(name, self.record)
        self.guest = ("browser: load " + self.record["url"]
                      + "\nARMED: browser application exit while native picker is open\n"
                      + "browser: WebKit Browser exiting\n")
        self.write("log.txt", self.guest)
        self.write("emulator.log", "normal main application exit\n")
        self.helper = ("BEGIN: browser-exit-request v1\n"
                       "TARGET: uid=e1a5c002 pid=245 name=C:\\sys\\bin\\browser.exe\n"
                       "REQUEST: TApaTask::EndTask; no KillTask, Terminate, or host WM close\n"
                       "REQUEST_SENT: official application shutdown event\n")
        self.write("app-exit-request.log", self.helper)

    def write(self, name, text):
        (self.path / name).write_text(text)

    def write_record(self, name, record):
        self.write(name, json.dumps(record))

    def result(self):
        return validator.validate(self.path, self.identities)["status"]

    def test_optional_helper_exit_missing(self):
        self.assertEqual(self.result(), "PASS")

    def test_helper_exit_normal(self):
        self.write("app-exit-request.log", self.helper + "EXIT: type=0 reason=0 logon=0\n")
        self.assertEqual(self.result(), "PASS")

    def test_host_crash(self):
        self.write_record("observation.json", dict(self.record, host_exit_code=-11))
        self.assertEqual(self.result(), "FAIL")

    def test_gdb_exit_zero_not_normal_host(self):
        self.write_record("observation.json", dict(self.record, diagnostic_gdb=True))
        self.assertEqual(self.result(), "FAIL")

    def test_no_destructor_marker(self):
        self.write("log.txt", self.guest.replace("browser: WebKit Browser exiting", ""))
        self.assertEqual(self.result(), "FAIL")

    def test_scenario_not_armed(self):
        self.write("log.txt", self.guest.replace("ARMED:", "NOT_ARMED:"))
        self.assertEqual(self.result(), "FAIL")

    def test_wrong_identity(self):
        self.write_record("launch.json", dict(self.record, browser_sha256="4" * 64))
        self.assertEqual(self.result(), "FAIL")

    def test_stale_page(self):
        self.write("log.txt", "case=exit&run=old\nbrowser: WebKit Browser exiting\n")
        self.assertEqual(self.result(), "FAIL")

    def test_wm_close_or_force(self):
        for field in ("wm_close_sent", "forced_cleanup"):
            with self.subTest(field=field):
                self.write_record("observation.json", dict(self.record, **{field: True}))
                self.assertEqual(self.result(), "FAIL")

    def test_helper_missing_request_or_wrong_target(self):
        for text in (self.helper.replace("REQUEST_SENT:", "UNSENT:"),
                     self.helper.replace("e1a5c002", "00000000"),
                     self.helper + "FAIL: browser-exit-request stage=timeout error=-33\n"):
            with self.subTest(text=text):
                self.write("app-exit-request.log", text)
                self.assertEqual(self.result(), "FAIL")

    def test_panic_even_with_host_zero(self):
        self.write("emulator.log", "Thread browser panicked with KERN-EXEC 3")
        self.assertEqual(self.result(), "FAIL")

    def test_abnormal_guest_exit(self):
        self.write("app-exit-request.log", self.helper + "EXIT: type=2 reason=3 logon=3\n")
        self.assertEqual(self.result(), "FAIL")


if __name__ == "__main__":
    unittest.main()
