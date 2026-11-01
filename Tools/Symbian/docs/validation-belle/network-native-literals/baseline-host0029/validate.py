#!/usr/bin/env python3
"""Validate this fixture's own log; runtime exit evidence remains separate."""

import argparse
import hashlib
import json
from pathlib import Path
import re

BASE_CASES = [
    "abi", "native-literal-v4", "native-literal-v6",
    "nameinfo-manual-v4", "nameinfo-manual-v6",
    "addrinfo-numeric-v4-af4", "addrinfo-numeric-v4-unspec",
    "addrinfo-numeric-v6-af6", "addrinfo-numeric-v6-unspec",
    "native-resolver-v4-literal", "native-resolver-v6-literal",
]
HOSTNAME_CASES = [
    "native-resolver-localhost", "addrinfo-localhost-af4",
    "addrinfo-localhost-af6", "addrinfo-localhost-unspec",
]


def families(name):
    if name == "abi":
        return False, False
    if name in ("native-resolver-localhost", "addrinfo-localhost-unspec"):
        return True, True
    return ("v4" in name or name.endswith("af4"),
            "v6" in name or name.endswith("af6"))


def validate(data, run, mode):
    """Require exact cases, results, run identity and flushed normal exit line."""
    if not re.fullmatch(r"[A-Za-z0-9_-]{8,64}", run):
        raise ValueError("invalid expected run")
    if mode not in ("all", "literals") or len(data) > 128 * 1024:
        raise ValueError("invalid mode or log bound")
    lines = data.decode("utf-8").splitlines()
    beginnings, cases, results, exits = [], [], [], []
    for line in lines:
        if line.startswith("BEGIN "):
            beginnings.append(json.loads(line[6:]))
        elif line.startswith("CASE "):
            cases.append(json.loads(line[5:]))
        elif line.startswith("RESULT "):
            results.append(json.loads(line[7:]))
        elif line.startswith("INFO "):
            continue
        elif line == "network-native-contract exiting 0":
            exits.append(line)
        else:
            raise ValueError("unknown or nonzero-exit log line")
    identity = {"suite": "network-native-contract", "version": 1,
                "run": run, "mode": mode}
    if beginnings != [identity] or not lines or not lines[0].startswith("BEGIN "):
        raise ValueError("fresh single BEGIN identity required")
    expected = BASE_CASES + (HOSTNAME_CASES if mode == "all" else [])
    if [item.get("name") for item in cases] != expected:
        raise ValueError("missing, duplicated, unordered or unexpected case")
    for item in cases:
        if set(item) != {"name", "pass", "rc", "count", "v4", "v6",
                         "valid", "complete"}:
            raise ValueError("case field shape differs")
        if any(item[key] is not True for key in ("pass", "valid", "complete")):
            raise ValueError("fixture reported failed or incomplete case")
        if any(type(item[key]) is not int for key in ("rc", "count", "v4", "v6")):
            raise ValueError("case numbers must be integers")
        expected_code = 1 if item["name"].startswith("nameinfo-manual-") else 0
        if item["rc"] != expected_code or not 0 <= item["count"] <= 16:
            raise ValueError("API return code or count violates contract")
        if item["v4"] < 0 or item["v6"] < 0 or item["v4"] + item["v6"] != item["count"]:
            raise ValueError("unclassified or invalid address count")
        if item["name"] == "abi":
            if item["count"]:
                raise ValueError("ABI record has runtime address count")
        elif not item["count"]:
            raise ValueError("successful address case cannot be empty")
        need_v4, need_v6 = families(item["name"])
        if bool(item["v4"]) != need_v4 or bool(item["v6"]) != need_v6:
            raise ValueError("case address families differ from fixed input")
    expected_summary = {**identity, "passed": len(expected), "failed": 0,
                        "total": len(expected)}
    if results != [expected_summary] or len(exits) != 1 or lines[-1] != exits[0]:
        raise ValueError("complete single result and final normal-exit line required")
    return {"status": "LOG_CONTRACT_PASS_PENDING_TARGET_EXIT",
            "suite": identity["suite"], "run": run, "mode": mode,
            "cases": len(expected), "log_sha256": hashlib.sha256(data).hexdigest(),
            "scope": "Fixture log only; root must independently verify target exit, host exit, pins and restoration"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--run", required=True)
    parser.add_argument("--mode", choices=("all", "literals"), default="all")
    args = parser.parse_args()
    try:
        report = validate(args.log.read_bytes(), args.run, args.mode)
    except (OSError, UnicodeError, ValueError, TypeError, KeyError) as error:
        print(json.dumps({"status": "FAIL", "error": str(error)}))
        return 1
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
