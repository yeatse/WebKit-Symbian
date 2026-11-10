#!/usr/bin/env python3
"""Validate logs only; the outer runner owns image hashes, locks and guest launch."""

import argparse
import json
import pathlib
import re


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate(browser, worker, token, script_url):
    require(re.fullmatch(r"[a-f0-9]{32}", token), "invalid run token")
    require(
        not re.search(
            r"access violation|KERN-EXEC|USER-EXEC|panicked|SIGSEGV",
            browser + worker,
            re.IGNORECASE,
        ),
        "panic/AV",
    )
    events = []
    for match in re.finditer(
        r"^console: "
        + re.escape(script_url)
        + r":\d+: MEMORY_PRESSURE_(READY|VERIFIED|PASS|FAIL): (.*)$",
        browser,
        re.MULTILINE,
    ):
        data = json.loads(match[2])
        require(data.get("token") == token, "stale/wrong-token page output")
        events.append((match[1], data, match.start()))
    ready = [event for event in events if event[0] == "READY"]
    passed = [event for event in events if event[0] == "PASS"]
    verified = [event for event in events if event[0] == "VERIFIED"]
    require(
        len(ready) == len(passed) == 1
        and not any(event[0] == "FAIL" for event in events),
        "unique READY/PASS without FAIL required",
    )
    begin, end = ready[0][2], passed[0][2]
    require(begin < end and len(verified) >= 2, "missing ordered live verification")
    require(
        all(begin < event[2] < end for event in verified),
        "verification outside retention window",
    )
    require(
        [event[1]["verifications"] for event in verified]
        == list(range(1, len(verified) + 1)),
        "missing/duplicate verification",
    )
    require(
        passed[0][1]["verifications"] == len(verified),
        "final verification count differs",
    )
    require(
        verified[-1][1]["elapsedMS"] >= 40000, "live retention shorter than 40 seconds"
    )
    for event in (ready[0], passed[0]):
        require(
            event[1]["count"] == 1572864 and event[1]["bufferBytes"] == 8388608,
            "wrong retained graph/buffer",
        )
    reliefs = list(
        re.finditer(
            r"^Symbian memory pressure: relief level=(\d+) systemFreeRAM=(\d+) valid=(\d+) nativeLow=(\d+)$",
            browser[begin:end],
            re.MULTILINE,
        )
    )
    require(1 <= len(reliefs) <= 4, "missing or unbounded pressure relief")
    real_low = [
        match
        for match in reliefs
        if match[1] == "2" and match[3] == "1" and match[4] == "1"
    ]
    require(real_low, "no actual native LowMemory-triggered critical relief")
    measured = []
    for match in real_low:
        absolute = begin + match.start()
        following = browser[absolute:end]
        before = re.search(
            r"^Symbian memory pressure: JS heap phase=before cellBytes=(\d+) extraBytes=(\d+)$",
            following,
            re.MULTILINE,
        )
        after = re.search(
            r"^Symbian memory pressure: JS heap phase=after-scheduling cellBytes=(\d+) extraBytes=(\d+)$",
            following,
            re.MULTILINE,
        )
        require(
            before and after and before.start() < after.start(),
            "missing actual VM heap observations",
        )
        require(int(before[1]) >= 80 * 1024 * 1024, "actual JSCell heap below 80 MiB")
        require(
            any(event[2] > absolute + after.end() for event in verified),
            "no full live verification after relief",
        )
        measured.append(int(before[1]))
    prefix = re.escape(token)
    require(
        len(
            re.findall(
                r"^START token=" + prefix + r" free=\d+ hardCap=260046848$",
                worker,
                re.MULTILINE,
            )
        )
        == 1,
        "unique bounded worker start required",
    )
    lows = re.findall(
        r"^LOW token=" + prefix + r" crossed=1 free=(\d+) total=(\d+)$",
        worker,
        re.MULTILINE,
    )
    require(
        len(lows) == 1
        and int(lows[0][0]) < 8388608
        and 0 < int(lows[0][1]) <= 260046848,
        "no measured worker low crossing",
    )
    require(
        len(
            re.findall(
                r"^WORKER_RESULT token="
                + prefix
                + r" accounting=PASS low=PASS pattern=PASS release=PASS$",
                worker,
                re.MULTILINE,
            )
        )
        == 1,
        "worker checks failed",
    )
    require(
        re.findall(r"^EXIT error=(-?\d+)$", worker, re.MULTILINE) == ["0"],
        "worker did not exit cleanly",
    )
    require("FAIL" not in worker, "worker reported failure")
    return {
        "status": "PASS",
        "token": token,
        "live_verifications": len(verified),
        "relief_calls": len(reliefs),
        "measured_cell_bytes": measured,
        "worker_committed_bytes": int(lows[0][1]),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser-log", type=pathlib.Path, required=True)
    parser.add_argument("--worker-log", type=pathlib.Path, required=True)
    parser.add_argument("--token", required=True)
    parser.add_argument("--script-url", required=True)
    args = parser.parse_args()
    print(
        json.dumps(
            validate(
                args.browser_log.read_text(),
                args.worker_log.read_text(),
                args.token,
                args.script_url,
            ),
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
