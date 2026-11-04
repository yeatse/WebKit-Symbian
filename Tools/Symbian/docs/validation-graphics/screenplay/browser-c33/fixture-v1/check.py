#!/usr/bin/env python3
"""Check public ScreenPlay page records only. Never starts or stops a browser."""
import argparse
import json
from pathlib import Path
import re
import sys

LABEL = "SYMBIAN SCREENPLAY "
BASE = "file:///C:/Data/WebKit/screenplay.html"
NAMES = ("opaque_patches", "font_layout", "overflow_clip", "overwhite_alpha",
         "dirty_updates", "css_motion", "dom_resize", "canvas2d", "webgl", "hit_test")
MAX_LOG_BYTES = 16 * 1024 * 1024
COMMON = {"version", "run", "seq", "event"}
FIELDS = {
    "start": {"scope", "canvas2d", "webgl", "tap"},
    "check": {"name", "outcome"},
    "dirty": {"index", "color"},
    "input": {"trusted", "inside"},
    "done": {"result", "passed", "failed", "skipped", "reason", "elapsedMs", "display", "trustedInput"},
}


class Invalid(ValueError):
    """Only fixed public error descriptions are emitted."""


def require(condition, message):
    if not condition:
        raise Invalid(message)


def token(value):
    require(isinstance(value, str) and re.fullmatch(r"[0-9a-f]{32}", value) is not None, "Invalid run token")
    return value


def url_for(run, canvas2d=True, webgl=False, tap=False):
    token(run)
    require(all(type(v) is bool for v in (canvas2d, webgl, tap)), "Invalid scope options")
    return f"{BASE}?run={run}&canvas2d={int(canvas2d)}&webgl={int(webgl)}&tap={int(tap)}"


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON key")
        result[key] = value
    return result


def no_constant(_):
    raise Invalid("Invalid JSON constant")


def decode(payload):
    require(len(payload) <= 2048, "Oversized public record")
    try:
        value = json.loads(payload, object_pairs_hook=unique_object, parse_constant=no_constant)
    except (ValueError, RecursionError):
        raise Invalid("Invalid public JSON") from None
    require(type(value) is dict, "Public record must be an object")
    require(type(value.get("event")) is str and value["event"] in FIELDS, "Unknown public event")
    require(set(value) == COMMON | FIELDS[value["event"]], "Unexpected public fields")
    return value


def validate(text, run, canvas2d=True, webgl=False, tap=False):
    expected_url = url_for(run, canvas2d, webgl, tap)
    pattern = re.compile(r"console: " + re.escape(expected_url) + r":[1-9][0-9]*: (.*)")
    records = []
    for line in text.splitlines():
        match = pattern.fullmatch(line)
        if match:
            require(match[1].startswith(LABEL), "Unrecognized console label for this page")
            records.append(decode(match[1][len(LABEL):]))
        elif LABEL in line:
            raise Invalid("Stale or unwrapped public record")
    require(1 <= len(records) <= 16, "Missing or excessive public records")
    checks, dirty, inputs = {}, [], []
    start = done = None
    for index, value in enumerate(records):
        require(type(value["version"]) is int and value["version"] == 1, "Unsupported public version")
        require(value["run"] == run, "Run token mismatch")
        require(type(value["seq"]) is int and value["seq"] == index, "Public sequence mismatch")
        require(done is None, "Record after terminal result")
        event = value["event"]
        if index == 0:
            require(event == "start", "Missing initial record")
        if event == "start":
            require(start is None and index == 0, "Duplicate or misplaced initial record")
            require(value["scope"] == "dom-paint-requests", "Unsupported evidence scope")
            require(all(type(value[k]) is bool for k in ("canvas2d", "webgl", "tap")), "Invalid scope types")
            require((value["canvas2d"], value["webgl"], value["tap"]) == (canvas2d, webgl, tap), "Scope mismatch")
            start = value
        elif event == "check":
            require(type(value["name"]) is str and value["name"] in NAMES, "Unknown check name")
            require(value["name"] not in checks, "Duplicate page check")
            require(value["outcome"] in ("pass", "fail", "skipped"), "Unknown check outcome")
            disabled = value["name"] == "canvas2d" and not canvas2d or value["name"] == "webgl" and not webgl
            require((value["outcome"] == "skipped") == disabled, "Incorrect optional scope outcome")
            if value["name"] == "dirty_updates" and value["outcome"] == "pass":
                require(len(dirty) == 3, "Dirty check precedes dirty requests")
            checks[value["name"]] = value["outcome"]
        elif event == "dirty":
            require(type(value["index"]) is int and value["index"] == len(dirty)+1, "Dirty update sequence mismatch")
            require(len(dirty) < 3, "Excessive dirty updates")
            require(value["color"] == ("orange", "purple", "cyan")[len(dirty)], "Unexpected dirty color")
            dirty.append(value["color"])
        elif event == "input":
            require(tap and not inputs, "Unexpected or duplicate input observation")
            require(type(value["trusted"]) is bool and type(value["inside"]) is bool, "Invalid input observation types")
            inputs.append({"trusted_observed": value["trusted"], "hit_test_observed": value["inside"]})
        elif event == "done":
            require(set(checks) == set(NAMES), "Incomplete named page checks")
            require(value["reason"] in ("complete", "watchdog", "runtime_exception"), "Unknown terminal reason")
            require(type(value["elapsedMs"]) is int and 0 <= value["elapsedMs"] <= 10000, "Page exceeded bounded duration")
            require(value["display"] == "UNVERIFIED" and value["trustedInput"] == "UNVERIFIED", "Unsupported proof claim")
            expected_counts = tuple(sum(v == outcome for v in checks.values()) for outcome in ("pass", "fail", "skipped"))
            require(all(type(value[k]) is int for k in ("passed", "failed", "skipped")), "Invalid terminal count types")
            require((value["passed"], value["failed"], value["skipped"]) == expected_counts, "Terminal counts disagree")
            expected_result = "PAGE_REQUESTS_PASS" if value["failed"] == 0 and value["reason"] == "complete" else "PAGE_REQUESTS_FAIL"
            require(value["result"] == expected_result, "Terminal result disagrees")
            done = value
    require(start is not None and done is not None, "Missing initial or terminal record")
    if done["result"] == "PAGE_REQUESTS_PASS":
        require(dirty == ["orange", "purple", "cyan"], "Incomplete dirty updates")
        require(done["elapsedMs"] >= (8000 if tap else 1800), "Terminal duration inconsistent with requested work")
    return {
        "schema": "symbian-screenplay-page-check-v1", "run": run, "url": expected_url,
        "page_status": done["result"], "reason": done["reason"], "elapsed_ms": done["elapsedMs"],
        "checks": checks, "dirty_updates": dirty, "input_observations": inputs,
        "display_status": "UNVERIFIED", "trusted_input_status": "UNVERIFIED",
        "runtime_status": "REQUIRES_SEPARATE_REVIEW", "native_finish_status": "NOT_PROVEN_BY_PAGE",
        "scope": "Only fresh-token public page DOM/layout and API request records; no display proof",
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("url", "check"))
    parser.add_argument("--run", required=True, help="Fresh externally generated 32 lowercase hexadecimal characters")
    parser.add_argument("--canvas2d", choices=("0", "1"), default="1")
    parser.add_argument("--webgl", choices=("0", "1"), default="0")
    parser.add_argument("--tap", choices=("0", "1"), default="0")
    parser.add_argument("--log", type=Path, help="Public console log; required for check")
    args = parser.parse_args(argv)
    options = {"canvas2d":args.canvas2d == "1", "webgl":args.webgl == "1", "tap":args.tap == "1"}
    try:
        expected_url = url_for(args.run, **options)
        if args.command == "url":
            require(args.log is None, "url does not accept a log")
            print(expected_url)
            return 0
        require(args.log is not None, "check requires a public log")
        require(args.log.stat().st_size <= MAX_LOG_BYTES, "Public log is too large")
        with args.log.open("rb") as stream:
            data = stream.read(MAX_LOG_BYTES+1)
        require(len(data) <= MAX_LOG_BYTES, "Public log is too large")
        result = validate(data.decode("utf-8"), args.run, **options)
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0 if result["page_status"] == "PAGE_REQUESTS_PASS" else 1
    except (Invalid, OSError, UnicodeError) as error:
        message = str(error) if isinstance(error, Invalid) else "Cannot read a bounded UTF-8 public log"
        print(json.dumps({"schema":"symbian-screenplay-page-check-v1", "page_status":"REJECTED", "error":message,
                          "display_status":"UNVERIFIED", "trusted_input_status":"UNVERIFIED"}, sort_keys=True))
        return 2


if __name__ == "__main__":
    sys.exit(main())
