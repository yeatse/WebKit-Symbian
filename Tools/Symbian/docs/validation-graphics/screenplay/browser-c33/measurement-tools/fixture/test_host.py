#!/usr/bin/env python3
"""Synthetic protocol and source checks only; never browser/display evidence."""
import copy
from html.parser import HTMLParser
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

import check

ROOT = Path(__file__).resolve().parent
RUN = "a" * 32  # Test-only token, never a live evidence identity.


def example(canvas2d=True, webgl=False, tap=False):
    rows = []
    def add(event, **fields):
        rows.append(dict(version=1, run=RUN, seq=len(rows), event=event, **fields))
    add("start", scope="dom-paint-requests", canvas2d=canvas2d, webgl=webgl, tap=tap)
    for index, color in enumerate(("orange", "purple", "cyan"), 1):
        add("dirty", index=index, color=color)
    if tap:
        add("input", trusted=True, inside=True)
    for name in check.NAMES:
        disabled = name == "canvas2d" and not canvas2d or name == "webgl" and not webgl
        add("check", name=name, outcome="skipped" if disabled else "pass")
    skipped = int(not canvas2d) + int(not webgl)
    add("done", result="PAGE_REQUESTS_PASS", passed=10-skipped, failed=0, skipped=skipped,
        reason="complete", elapsedMs=8500 if tap else 2500, display="UNVERIFIED", trustedInput="UNVERIFIED")
    return rows


def wrapped(rows, **options):
    url = check.url_for(RUN, **options)
    return "\n".join("console: " + url + ":100: " + check.LABEL + json.dumps(value) for value in rows) + "\n"


class Protocol(unittest.TestCase):
    def reject(self, rows, **options):
        with self.assertRaises(check.Invalid):
            check.validate(wrapped(rows, **options), RUN, **options)

    def test_baseline(self):
        report = check.validate(wrapped(example()), RUN)
        self.assertEqual(report["page_status"], "PAGE_REQUESTS_PASS")
        self.assertEqual(report["display_status"], "UNVERIFIED")
        self.assertEqual(report["trusted_input_status"], "UNVERIFIED")

    def test_every_scope(self):
        for canvas2d in (False, True):
            for webgl in (False, True):
                for tap in (False, True):
                    with self.subTest(canvas2d=canvas2d, webgl=webgl, tap=tap):
                        options = dict(canvas2d=canvas2d, webgl=webgl, tap=tap)
                        report = check.validate(wrapped(example(**options), **options), RUN, **options)
                        self.assertEqual(report["trusted_input_status"], "UNVERIFIED")

    def test_stale_token(self):
        with self.assertRaises(check.Invalid):
            check.validate(wrapped(example()), "b"*32)

    def test_bad_scope(self):
        rows = example()
        rows[0]["webgl"] = True
        self.reject(rows)

    def test_extra_private_field_is_rejected_without_echo(self):
        rows = example()
        rows[0]["unknown"] = "PRIVATE_VALUE_SENTINEL"
        with self.assertRaises(check.Invalid) as caught:
            check.validate(wrapped(rows), RUN)
        self.assertNotIn("PRIVATE_VALUE_SENTINEL", str(caught.exception))

    def test_duplicate_json_key(self):
        log = wrapped(example()).replace('"version": 1', '"version": 1, "version": 1', 1)
        with self.assertRaises(check.Invalid):
            check.validate(log, RUN)

    def test_nonfinite_json(self):
        log = wrapped(example()).replace('"version": 1', '"version": NaN', 1)
        with self.assertRaises(check.Invalid):
            check.validate(log, RUN)

    def test_sequence(self):
        rows = example()
        rows[2]["seq"] = 0
        self.reject(rows)

    def test_missing_terminal(self):
        self.reject(example()[:-1])

    def test_duplicate_check(self):
        rows = example()
        rows[5]["name"] = rows[4]["name"]
        self.reject(rows)

    def test_unsupported_proof(self):
        for key in ("display", "trustedInput"):
            rows = example()
            rows[-1][key] = "VERIFIED"
            self.reject(rows)

    def test_terminal_counts(self):
        rows = example()
        rows[-1]["passed"] = 10
        self.reject(rows)

    def test_duration(self):
        for duration in (0, 10001, True):
            rows = example()
            rows[-1]["elapsedMs"] = duration
            self.reject(rows)

    def test_unwrapped_or_other_label(self):
        for log in (wrapped(example()).replace("console: ", "", 1),
                    wrapped(example()).replace(check.LABEL, "UNKNOWN ", 1)):
            with self.assertRaises(check.Invalid):
                check.validate(log, RUN)

    def test_record_after_done(self):
        rows = example()
        extra = copy.deepcopy(rows[0])
        extra["seq"] = len(rows)
        rows.append(extra)
        self.reject(rows)

    def test_wrong_dirty_sequence_and_color(self):
        for key, bad in (("index", 2), ("color", "red")):
            rows = example()
            rows[1][key] = bad
            self.reject(rows)

    def test_input_only_when_enabled(self):
        rows = example(tap=True)
        rows[0]["tap"] = False
        rows[-1]["elapsedMs"] = 2500
        self.reject(rows)

    def test_page_failure_is_separate_from_malformed(self):
        rows = example()
        rows[4]["outcome"] = "fail"
        rows[-1].update(result="PAGE_REQUESTS_FAIL", passed=8, failed=1)
        report = check.validate(wrapped(rows), RUN)
        self.assertEqual(report["page_status"], "PAGE_REQUESTS_FAIL")
        self.assertEqual(report["display_status"], "UNVERIFIED")

    def test_runtime_exception_can_finish_with_disabled_scope(self):
        rows = example()
        for row in rows:
            if row["event"] == "check" and row["outcome"] == "pass":
                row["outcome"] = "fail"
        rows[-1].update(result="PAGE_REQUESTS_FAIL", passed=0, failed=9, reason="runtime_exception", elapsedMs=10)
        report = check.validate(wrapped(rows), RUN)
        self.assertEqual(report["reason"], "runtime_exception")

    def test_unknown_event_or_check(self):
        rows = example()
        rows[1]["event"] = "unknown"
        self.reject(rows)
        rows = example()
        rows[4]["name"] = "unknown"
        self.reject(rows)

    def test_boolean_not_numeric_identity(self):
        for key in ("version", "seq"):
            rows = example()
            rows[0][key] = True
            self.reject(rows)

    def test_cli_missing_log_no_path_echo(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "UNPUBLISHED_PATH_SENTINEL"
            result = subprocess.run(["python3", str(ROOT/"check.py"), "check", "--run", RUN, "--log", str(log)],
                                    capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 2)
            self.assertNotIn("UNPUBLISHED_PATH_SENTINEL", result.stdout + result.stderr)


class Source(HTMLParser):
    def __init__(self):
        super().__init__()
        self.script = False
        self.scripts = []
        self.external = []
    def handle_starttag(self, tag, attrs):
        self.script = tag == "script"
        for key, value in attrs:
            if key in ("src", "href", "action", "poster"):
                self.external.append((key, value))
    def handle_endtag(self, tag):
        if tag == "script":
            self.script = False
    def handle_data(self, data):
        if self.script:
            self.scripts.append(data)


class FixtureSource(unittest.TestCase):
    def test_manifest_protocol_and_roi_bounds(self):
        manifest = json.loads((ROOT/"manifest.json").read_text())
        self.assertEqual(manifest["named_checks"], list(check.NAMES))
        self.assertEqual(manifest["page_console_label"], check.LABEL)
        self.assertEqual(manifest["target"], check.BASE)
        self.assertEqual(manifest["bounds_ms"]["maximum_accepted_elapsed"], 10000)
        sx, sy, sw, sh = manifest["screenshot_coordinates"]["scene_rect"]
        for patch in manifest["screenshot_coordinates"]["patches"]:
            x, y, width, height = patch["roi"]
            self.assertGreater(width, 0)
            self.assertGreater(height, 0)
            self.assertTrue(sx <= x < x+width <= sx+sw and sy <= y < y+height <= sy+sh)
            self.assertEqual(len(patch["expected_rgb"]), 3)
            self.assertTrue(all(type(value) is int and 0 <= value <= 255 for value in patch["expected_rgb"]))

    def test_offline_source_and_no_self_input_or_shutdown(self):
        html = (ROOT/"screenplay.html").read_text()
        source = Source()
        source.feed(html)
        self.assertEqual(source.external, [])
        self.assertEqual(len(source.scripts), 1)
        for forbidden in ("fetch(", "XMLHttpRequest", "WebSocket", "sendBeacon", "window.close", ".click(", "dispatchEvent(", "clipboard", "localStorage", "document.cookie"):
            self.assertNotIn(forbidden, html)
        self.assertNotIn("url(", html)

    @unittest.skipUnless(shutil.which("node"), "Node unavailable; JS syntax needs a separate check")
    def test_javascript_syntax_only(self):
        source = Source()
        source.feed((ROOT/"screenplay.html").read_text())
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory)/"fixture.js"
            script.write_text(source.scripts[0])
            result = subprocess.run(["node", "--check", str(script)], capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, "Fixture JavaScript syntax check failed")

    @unittest.skipUnless(importlib.util.find_spec("jsonschema"), "jsonschema unavailable; parser still enforces protocol")
    def test_schema_for_all_scopes(self):
        import jsonschema
        schema = json.loads((ROOT/"record.schema.json").read_text())
        jsonschema.Draft202012Validator.check_schema(schema)
        for canvas2d in (False, True):
            for webgl in (False, True):
                for tap in (False, True):
                    for row in example(canvas2d, webgl, tap):
                        jsonschema.validate(row, schema)


if __name__ == "__main__":
    unittest.main(verbosity=2)
