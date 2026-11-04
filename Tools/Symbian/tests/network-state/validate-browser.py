#!/usr/bin/env python3
"""Validate fresh browser events and the real connection controller transcript."""

import argparse
import hashlib
import json
import re
from pathlib import Path


def expected_checks():
    names = ["initial native interface online"]
    for number, event in enumerate(("offline", "online", "offline", "online"), 1):
        names.extend(
            [
                f"event {number} is {event}",
                f"event {number} navigator state",
                f"event {number} trusted window dispatch",
                f"event {number} stable without duplicates",
            ]
        )
        if number == 2:
            names.append("normal attachment churn keeps online without events")
    names.append("exact ordered transition sequence")
    return names


def validate(browser, controller, token):
    if not re.fullmatch(r"[A-Za-z0-9_-]{8,48}", token):
        raise ValueError("invalid fresh run token")
    details = re.findall(r"NETWORKSTATE DETAILS (\{[^\r\n]*\})", browser)
    if len(details) != 1:
        raise ValueError("expected exactly one complete details record")
    report = json.loads(details[0])
    if report.get("run") != token:
        raise ValueError("stale browser report")
    if [report.get(name) for name in ("passed", "failed", "total")] != [19, 0, 19]:
        raise ValueError("browser did not pass all 19 checks")
    checks = report.get("checks", [])
    if [entry.get("name") for entry in checks] != expected_checks() or any(
        entry.get("passed") is not True for entry in checks
    ):
        raise ValueError("missing, duplicate, reordered or failed browser check")
    events = report.get("events", [])
    if len(events) != 4:
        raise ValueError("unexpected event count")
    last_time = -1
    for entry, event in zip(events, ("offline", "online", "offline", "online")):
        if entry.get("type") != event or entry.get("online") is not (event == "online"):
            raise ValueError("wrong event order or observed navigator state")
        if entry.get("trusted") is not True or entry.get("windowTarget") is not True:
            raise ValueError("event was not a trusted window dispatch")
        elapsed = entry.get("elapsed")
        if type(elapsed) not in (int, float) or not last_time < elapsed <= 90000:
            raise ValueError("invalid event time")
        last_time = elapsed
    if not isinstance(report.get("startupEvents"), list):
        raise ValueError("startup observations missing")  # noqa: TRY004 - invalid report
    phases = ("BEGIN", "READY", "OFFLINE1", "ONLINE1", "STABLE1", "OFFLINE2", "DONE")
    actual = re.findall(
        r"NETWORKSTATE (BEGIN|READY|OFFLINE1|ONLINE1|STABLE1|OFFLINE2|DONE) ([A-Za-z0-9_-]+)(?=\r?$)",
        browser,
        re.MULTILINE,
    )
    if actual != [(phase, token) for phase in phases]:
        raise ValueError("browser phase sequence is stale or incomplete")
    results = re.findall(
        r"NETWORKSTATE RESULT ([A-Za-z0-9_-]+) passed=(\d+) failed=(\d+)", browser
    )
    if results != [(token, "19", "0")]:
        raise ValueError("missing or repeated browser result")
    begins = re.findall(
        r"^BEGIN: network-state-controller version=1 run=(\S+)$",
        controller,
        re.MULTILINE,
    )
    if begins != [token]:
        raise ValueError("stale or repeated controller start")
    acknowledgements = re.findall(
        r"^ACK: NETWORKSTATE (\w+) (\S+)$", controller, re.MULTILINE
    )
    if acknowledgements != [(phase, token) for phase in phases[1:]]:
        raise ValueError("controller did not observe the browser transitions")
    for operation in (
        "stop first interface",
        "start first reconnect",
        "extra normal attachment and close",
        "stop second interface",
        "start second reconnect",
    ):
        if controller.count(f"CONTROL: {operation} rc=0\n") != 1:
            raise ValueError(f"missing successful native operation: {operation}")
    counts = re.findall(
        r"^CONTROL: active interfaces=(\d+) expected=(\d+)$", controller, re.MULTILINE
    )
    if counts != [(str(count), str(count)) for count in (0, 1, 0, 1, 1, 0, 1)]:
        raise ValueError("connection counts do not match actual stop/start sequence")
    required = (
        "BROWSER EXIT: type=0 reason=0 logon=0",
        "CONTROL: sequence error=0 browser-close error=0",
        "RESULT: passed=1 failed=0",
        "network-state-controller exiting 0",
    )
    for line in required:
        if (
            len(re.findall(r"^" + re.escape(line) + r"$", controller, re.MULTILINE))
            != 1
        ):
            raise ValueError(f"missing strict normal-exit evidence: {line}")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser-log", required=True, type=Path)
    parser.add_argument("--controller-log", required=True, type=Path)
    parser.add_argument("--run", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    report = {
        "status": "FAIL",
        "scope": "Network state page results",
        "run": args.run,
    }
    try:
        browser = args.browser_log.read_bytes()
        controller = args.controller_log.read_bytes()
        report["inputs"] = {
            "browser_sha256": hashlib.sha256(browser).hexdigest(),
            "controller_sha256": hashlib.sha256(controller).hexdigest(),
        }
        report["details"] = validate(
            browser.decode("utf-8"), controller.decode("utf-8"), args.run
        )
        report["status"] = "PASS"
    except (ValueError, OSError, UnicodeError) as error:
        report["error"] = str(error)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in ("status", "run")}))
    raise SystemExit(report["status"] != "PASS")


if __name__ == "__main__":
    main()
