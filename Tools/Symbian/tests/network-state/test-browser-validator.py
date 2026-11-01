#!/usr/bin/env python3
"""Positive and fail-closed contract checks for the log validator, no guest."""

import copy
import importlib.util
import json
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "validator", Path(__file__).with_name("validate-browser.py")
)
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)
TOKEN = "fresh-run-1234"
PHASES = ("BEGIN", "READY", "OFFLINE1", "ONLINE1", "STABLE1", "OFFLINE2", "DONE")


def reports():
    report = {
        "run": TOKEN,
        "passed": 19,
        "failed": 0,
        "total": 19,
        "checks": [
            {"name": name, "passed": True} for name in validator.expected_checks()
        ],
        "events": [
            {
                "type": event,
                "online": event == "online",
                "trusted": True,
                "windowTarget": True,
                "elapsed": (index + 1) * 6000,
            }
            for index, event in enumerate(("offline", "online", "offline", "online"))
        ],
        "startupEvents": [],
    }
    controller = f"BEGIN: network-state-controller version=1 run={TOKEN}\n"
    controller += "".join(
        f"ACK: NETWORKSTATE {phase} {TOKEN}\n" for phase in PHASES[1:]
    )
    controller += "".join(
        f"CONTROL: {operation} rc=0\n"
        for operation in (
            "stop first interface",
            "start first reconnect",
            "extra normal attachment and close",
            "stop second interface",
            "start second reconnect",
        )
    )
    controller += "".join(
        f"CONTROL: active interfaces={count} expected={count}\n"
        for count in (0, 1, 0, 1, 1, 0, 1)
    )
    controller += "BROWSER EXIT: type=0 reason=0 logon=0\nCONTROL: sequence error=0 browser-close error=0\nRESULT: passed=1 failed=0\nnetwork-state-controller exiting 0\n"
    return report, controller


def browser(report):
    return (
        "".join(
            f"console: file:///fixture.html:1: NETWORKSTATE {phase} {TOKEN}\n"
            for phase in PHASES
        )
        + "NETWORKSTATE DETAILS "
        + json.dumps(report)
        + f"\nNETWORKSTATE RESULT {TOKEN} passed=19 failed=0\n"
    )


class ValidatorTests(unittest.TestCase):
    def test_complete_record(self):
        report, controller = reports()
        self.assertEqual(validator.validate(browser(report), controller, TOKEN), report)

    def test_stale_token(self):
        report, controller = reports()
        with self.assertRaisesRegex(ValueError, "stale"):
            validator.validate(browser(report), controller, "different-run")

    def test_duplicate_details(self):
        report, controller = reports()
        with self.assertRaises(ValueError):
            validator.validate(browser(report) + browser(report), controller, TOKEN)

    def test_report_mutations(self):
        original, controller = reports()
        mutations = (
            lambda r: r.update(passed=18),
            lambda r: r["checks"][0].update(passed=False),
            lambda r: r["checks"].reverse(),
            lambda r: r["events"].append(copy.deepcopy(r["events"][-1])),
            lambda r: r["events"][0].update(type="online"),
            lambda r: r["events"][0].update(online=True),
            lambda r: r["events"][0].update(trusted=False),
            lambda r: r["events"][0].update(windowTarget=False),
            lambda r: r["events"][1].update(elapsed=0),
            lambda r: r["events"][1].update(elapsed=float("nan")),
            lambda r: r.pop("startupEvents"),
        )
        for mutation in mutations:
            report = copy.deepcopy(original)
            mutation(report)
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                validator.validate(browser(report), controller, TOKEN)

    def test_controller_failures(self):
        report, controller = reports()
        for old, new in (
            ("stop first interface rc=0", "stop first interface rc=-1"),
            ("active interfaces=0 expected=0", "active interfaces=1 expected=0"),
            ("ACK: NETWORKSTATE OFFLINE2", "ACK: NETWORKSTATE OFFLINE1"),
            ("BROWSER EXIT: type=0 reason=0", "BROWSER EXIT: type=2 reason=3"),
            ("browser-close error=0", "browser-close error=-33"),
            (
                "network-state-controller exiting 0",
                "network-state-controller exiting 1",
            ),
        ):
            with self.subTest(old=old), self.assertRaises(ValueError):
                validator.validate(browser(report), controller.replace(old, new), TOKEN)


if __name__ == "__main__":
    unittest.main(verbosity=2)
