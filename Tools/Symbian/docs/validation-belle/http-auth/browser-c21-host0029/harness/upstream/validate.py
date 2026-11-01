#!/usr/bin/env python3
"""Validate redacted fixture/input evidence. Never launches a process."""

import argparse
import json
from pathlib import Path
import re

CRASH = re.compile(r"KERN-EXEC|USER-EXEC|\bPANIC\b|panicked|access violation|SIGSEGV|segmentation fault", re.I)


def validate(fixture, interaction, guest=None, host=None, report=None, helper=None):
    ready = json.loads((fixture / "ready.json").read_text())
    events = [json.loads(line) for line in (fixture / "events.jsonl").read_text().splitlines()]
    drive = json.loads(interaction.read_text())
    failures, incomplete = [], []
    for item in events + [drive]:
        if any(item.get(name) != ready[name] for name in ("run", "scheme", "case")):
            failures.append("evidence identity mismatch")
            break
    if [e.get("sequence") for e in events] != list(range(1, len(events) + 1)):
        failures.append("fixture event sequence is incomplete")
    if drive.get("status") == "FAIL":
        failures.append("native interaction failed")
    if drive.get("screenshots_taken") is not False:
        failures.append("interaction must explicitly avoid secret screenshots")
    case = ready["case"]
    expected_status = "EXIT_REQUEST_SENT_PENDING_VALIDATION" if case.startswith("exit") else "INTERACTION_COMPLETE_PENDING_VALIDATION"
    if drive.get("status") != expected_status:
        failures.append("native input did not finish the required case")
    requests = [e for e in events if e["event"] == "auth-request"]
    submitted = [e for e in requests if e.get("authorization_present")]
    accepted = [e for e in requests if e.get("accepted")]
    pages = [e["event"] for e in events if e["event"].startswith("page-")]
    records = drive.get("records", [])
    input_events = [r["event"] for r in records]
    if "page-ready" not in pages or pages.count("page-started") != 1 or not requests:
        failures.append("missing fresh page start/challenge")
    if case == "success":
        if len(submitted) != 1 or len(accepted) != 1 or pages.count("page-loaded") != 1:
            failures.append("successful case needs one accepted credential and response page")
        if input_events.count("credential-submitted") != 1:
            failures.append("successful case needs one real input submission")
    elif case == "retry":
        if len(submitted) != 3 or accepted or pages.count("page-exhausted") != 1 or "page-loaded" in pages:
            failures.append("retry case needs exactly three rejected submissions and final 401 page")
        attempts = [r.get("attempt") for r in records if r["event"] == "credential-submitted"]
        if attempts != [1, 2, 3]:
            failures.append("retry input must submit exactly attempts 1/2/3")
    else:
        if submitted or accepted or "page-loaded" in pages:
            failures.append("cancel/lifecycle case must not transmit any credential")
        if "native-query-pending" not in input_events:
            failures.append("pending native query was not recorded")
        if case.startswith("cancel") and input_events.count("native-cancel") != 1:
            failures.append("explicit native Cancel is absent")
        if case.startswith("navigate"):
            if "navigation-requested" not in input_events or pages.count("page-navigated") != 1:
                failures.append("navigation did not complete")
        if case.startswith("exit") and "normal-exit-helper-launch" not in input_events:
            failures.append("normal guest exit helper launch is absent")
    if not case.startswith("exit"):
        quiet = [r for r in records if r["event"] == "quiet-observation"]
        if len(quiet) != 1 or quiet[0].get("seconds", 0) < 3:
            failures.append("bounded quiet observation is absent")
        if not case.startswith("navigate") and "page-responsive" not in pages:
            failures.append("parent page did not respond after the query")
    supplied_text = (fixture / "events.jsonl").read_text() + interaction.read_text()
    guest_text = guest.read_text(errors="replace") if guest else ""
    host_text = host.read_text(errors="replace") if host else ""
    supplied_text += guest_text + host_text
    credentials = json.loads((fixture / "credentials.private.json").read_text())
    if any(credentials[name] in supplied_text for name in ("password", "wrong_password")):
        failures.append("a synthetic password appeared in evidence; retain only redacted evidence")
    if re.search(r"\bAuthorization\s*:\s*(?:Basic|Digest)\b|curl trace on", supplied_text, re.I):
        failures.append("authentication trace appeared in evidence")
    if CRASH.search(guest_text + "\n" + host_text):
        failures.append("guest panic or host crash marker is present")
    if not guest or not host or not report:
        incomplete.append("fresh guest/host logs and runner observation are required for integrated PASS")
    else:
        observation = json.loads(report.read_text())
        if observation.get("run") != ready["run"] or observation.get("url") != ready["url"]:
            failures.append("runner run/URL differs from fixture")
        if ready["run"] not in guest_text:
            failures.append("fresh fixture token absent from guest log")
        if observation.get("host_exit_code") != 0 or observation.get("normal_close") is not True:
            failures.append("runner did not record normal host/guest closure")
        if observation.get("forced_cleanup") is not False or not observation.get("finished_utc"):
            failures.append("runner observation is incomplete or used forced cleanup")
        if observation.get("curl_trace_disabled") is not True:
            incomplete.append("runner must confirm curl tracing was disabled before input")
        for name in ("browser_sha256", "emulator_sha256"):
            if not re.fullmatch(r"[0-9a-f]{64}", observation.get(name, "")):
                failures.append("missing runner image identity: " + name)
        # Input automation proves delivery, not the text/appearance of AVKON.
        if observation.get("native_query_reviewed") is not True:
            incomplete.append("operator must confirm HTTP sign-in heading/origin and correct native query")
        if not case.startswith("exit") and observation.get("native_query_dismissed_reviewed") is not True:
            incomplete.append("operator must confirm the native authentication query closed at the end")
        if case in ("success", "retry") and observation.get("masked_password_reviewed") is not True:
            incomplete.append("operator must confirm native masked password input")
        if case.startswith("exit"):
            if observation.get("wm_close_sent") is not False:
                failures.append("exit acceptance cannot use a host WM close")
            if not helper:
                incomplete.append("normal EndTask helper log is required")
            else:
                helper_text = helper.read_text(errors="replace")
                if helper_text.count("BEGIN: browser-exit-request v1") != 1:
                    failures.append("one fresh EndTask helper run is required")
                if not re.search(r"^TARGET: uid=e1a5c002 pid=[1-9][0-9]* name=[A-Za-z]:\\sys\\bin\\browser\.exe$", helper_text, re.M | re.I):
                    failures.append("helper browser identity absent")
                if "REQUEST_SENT: official application shutdown event" not in helper_text or "FAIL:" in helper_text:
                    failures.append("official shutdown was not successfully requested")
                exits = re.findall(r"^EXIT: type=(-?\d+) reason=(-?\d+) logon=(-?\d+)$", helper_text, re.M)
                if exits and exits != [("0", "0", "0")]:
                    failures.append("helper observed abnormal guest exit")
                if len(re.findall(r"^[0-2][0-9]:[0-5][0-9]:[0-5][0-9]\.[0-9]{3} WebKit Browser exiting$", guest_text, re.M)) != 1:
                    failures.append("native browser destructor completion marker absent")
    return {"status": "FAIL" if failures else "INCOMPLETE" if incomplete else "PASS",
        "run": ready["run"], "scheme": ready["scheme"], "case": case,
        "functional_checks": "FAIL" if failures else "PASS", "failures": failures,
        "incomplete": incomplete, "auth_requests": len(requests),
        "credential_submissions": len(submitted), "accepted": len(accepted),
        "scope": "controlled loopback HTTP authentication and real native UI input only"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--interaction", type=Path, required=True)
    parser.add_argument("--guest-log", type=Path)
    parser.add_argument("--emulator-log", type=Path)
    parser.add_argument("--runner-report", type=Path)
    parser.add_argument("--exit-helper-log", type=Path)
    args = parser.parse_args()
    try:
        result = validate(args.fixture, args.interaction, args.guest_log, args.emulator_log,
                          args.runner_report, args.exit_helper_log)
    except (OSError, ValueError, TypeError, KeyError) as error:
        result = {"status": "FAIL", "failures": [type(error).__name__ + ": malformed or missing evidence"]}
    print(json.dumps(result, indent=2))
    return {"PASS": 0, "FAIL": 1, "INCOMPLETE": 2}[result["status"]]


if __name__ == "__main__":
    raise SystemExit(main())
