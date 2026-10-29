#!/usr/bin/env python3
"""Read an EndTask observation; never launch or stop a process."""

import argparse
import gzip
import json
from pathlib import Path
import re


def read_text(directory, name):
    path = directory / name
    if path.exists():
        return path.read_text(encoding="utf-8", errors="replace")
    return gzip.decompress((directory / (name + ".gz")).read_bytes()).decode("utf-8", errors="replace")


def validate(directory, identities):
    failures = []
    observation = json.loads((directory / "observation.json").read_text())
    launch = json.loads((directory / "launch.json").read_text())
    guest = read_text(directory, "log.txt")
    host = read_text(directory, "emulator.log")
    helper = read_text(directory, "app-exit-request.log")
    for name, expected in identities.items():
        if not re.fullmatch(r"[0-9a-f]{64}", expected):
            failures.append(f"invalid expected {name}")
        for record, label in ((launch, "launch"), (observation, "observation")):
            if record.get(name) != expected:
                failures.append(f"{label} {name} mismatch")
    for name in ("wm_close_sent", "forced_cleanup"):
        if launch.get(name) is not False or observation.get(name) is not False:
            failures.append(f"{name} must explicitly be false")
    if observation.get("host_exit_code") != 0 or observation.get("diagnostic_gdb"):
        failures.append("a normal direct host exit 0 is required (GDB exit 0 is insufficient)")
    if not observation.get("finished_utc"):
        failures.append("observation did not finish")
    url = launch.get("url", "")
    if not url or observation.get("url") != url or url not in guest or "case=exit" not in url:
        failures.append("fresh exit-page URL is absent or inconsistent")
    if not re.search(r"\bARMED: browser application exit while native picker is open", guest):
        failures.append("native-picker exit scenario was not armed")
    if helper.count("BEGIN: browser-exit-request v1") != 1:
        failures.append("expected exactly one fresh helper run")
    targets = re.findall(r"^TARGET: uid=e1a5c002 pid=([0-9]+) name=([A-Za-z]:\\sys\\bin\\browser\.exe)$", helper, re.M | re.I)
    if len(targets) != 1 or int(targets[0][0]) <= 0:
        failures.append("helper did not verify the browser UID, process and executable")
    if "REQUEST: TApaTask::EndTask; no KillTask, Terminate, or host WM close" not in helper:
        failures.append("official EndTask request is absent")
    if "REQUEST_SENT: official application shutdown event" not in helper:
        failures.append("EndTask request was not flushed")
    if "FAIL:" in helper:
        failures.append("helper reported failure")
    exits = re.findall(r"^EXIT: type=(-?\d+) reason=(-?\d+) logon=(-?\d+)$", helper, re.M)
    if exits and exits != [("0", "0", "0")]:
        failures.append("helper observed an abnormal guest exit")
    if guest.count("browser: WebKit Browser exiting") != 1:
        failures.append("browser destructor completion marker is missing or duplicated")
    if re.search(r"KERN-EXEC|USER-EXEC|\bPANIC\b|panicked|access violation|SIGSEGV|segmentation fault", guest + "\n" + host, re.I):
        failures.append("guest panic/access violation or host crash is present")
    return {"status": "FAIL" if failures else "PASS", "failures": failures,
            "identities": identities, "helper_exit_line_present": bool(exits),
            "scope": "official EndTask with native picker open; no WM close or forced cleanup"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    for name in ("browser", "emulator", "helper"):
        parser.add_argument(f"--{name}-sha256", required=True)
    args = parser.parse_args()
    try:
        result = validate(args.directory, {name + "_sha256": getattr(args, name + "_sha256")
                                          for name in ("browser", "emulator", "helper")})
    except (OSError, ValueError, TypeError) as error:
        result = {"status": "FAIL", "failures": [str(error)]}
    print(json.dumps(result, indent=2))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
