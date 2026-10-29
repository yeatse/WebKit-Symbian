#!/usr/bin/env python3
"""Strict report validator; does not launch a browser or modify guest state."""

import argparse
import json
from collections import Counter
from pathlib import Path
from urllib.parse import urlsplit

INITIAL_NAMES = (
    "top-level DOM reads Strict/Lax/default; HttpOnly hidden; None without Secure rejected",
    "top-level DOM writes preserve SameSite; None without Secure rejected",
    "same-site fetch sends every policy",
    "cross-port same-site control sends every policy",
    "cross-site fetch GET sends only None",
    "cross-site fetch POST sends only None",
    "cross-site iframe request and DOM read/send restrictions",
    "same-site WebSocket includes Strict/Lax",
    "cross-site WebSocket includes only None",
    "fetch redirect A to B to A keeps cross-site taint",
    "cross-site response may set None; Strict/Lax/default deferred rejection verified on top-level B",
    "A to B to A iframe ancestry stays cross-site for DOM, fetch and WebSocket",
    "cross-site top-level GET permits Lax/default/None, rejects Strict",
    "top-level DOM can read Strict after cross-site navigation",
    "cross-site iframe DOM writes really rejected, not merely hidden",
    "cross-site subresource Set-Cookie Strict/Lax/default really rejected",
    "fresh default cookie excluded on top-level cross-site POST (no two-minute exception)",
    "DOM-created cookie policies enforced for cross-site POST",
    "same-site top-level GET permits Strict",
    "307 preserves cross-site POST restrictions",
    "top-level A to B to A GET chain still excludes Strict",
    "HTTP to HTTPS same-host POST is schemefully cross-site",
    "HTTP to HTTPS same-host GET permits Lax, excludes Strict",
)
RESUME_NAMES = (
    "restart A preserves policy and persistent cookies, removes session cookie",
    "restart preserves DOM-created cookie policy",
    "restart cross-site request still filters Strict/Lax/default",
    "restart B cross-site top-level request still filters Strict",
    "restart B DOM persistent state and no session cookie",
)
BASE_KEYS = {
    "strict",
    "lax",
    "none",
    "default",
    "mixed",
    "unknown",
    "duplicate",
    "session",
    "httpOnly",
    "badnone",
}
ALL = BASE_KEYS - {"badnone"}
LAX = {"lax", "none", "default", "unknown", "duplicate"}


def require(value, message):
    if not value:
        raise ValueError(message)


def validate_report(report, token, phase="initial", not_before_ns=None):
    """Accept a parsed dict or JSON path; return summary or raise ValueError.

    initial requires exactly 23 current outcomes; resumed requires those same
    23 plus exactly 5 restart outcomes. not_before_ns is the host launch bound.
    Process closure/restart and guest CA restoration remain runner obligations.
    """
    if not isinstance(report, dict):
        report = json.loads(Path(report).read_text())
    require(phase in ("initial", "resumed"), "invalid validation phase")
    config = report.get("config", {})
    require(
        report.get("suite") == "cookie-samesite" and config.get("run") == token,
        "suite/run mismatch",
    )
    a, b, other, http_a = [
        urlsplit(config.get(key, ""))
        for key in ("a", "b", "sameSiteOtherPort", "httpA")
    ]
    require(
        a.scheme == b.scheme == other.scheme == "https" and http_a.scheme == "http",
        "missing HTTPS and cross-scheme fixtures",
    )
    require(
        a.hostname == other.hostname == http_a.hostname == "localhost"
        and b.hostname == "127.0.0.1",
        "sites are not the required localhost/IP pair",
    )
    require(
        a.port == b.port
        and a.port != other.port
        and http_a.port not in (a.port, other.port),
        "invalid port control",
    )
    results = report.get("results", [])
    names = INITIAL_NAMES + (RESUME_NAMES if phase == "resumed" else ())
    require(
        Counter(row.get("name") for row in results) == Counter(names),
        "missing, duplicate or unexpected outcome",
    )
    for index, name in enumerate(names):
        row = next(row for row in results if row["name"] == name)
        expected_phase = (
            "main"
            if index < 12
            else "nav"
            if index < 23
            else "resume"
            if index < 26
            else "resume-b"
        )
        require(row.get("phase") == expected_phase, "wrong phase: " + name)
        require(
            row.get("status") == "PASS" and not row.get("detail"),
            "failed outcome: " + name + ": " + str(row.get("detail")),
        )
    requests = report.get("requests", [])
    require(bool(requests), "no wire observations")
    require(
        all(row.get("query", {}).get("run") == token for row in requests),
        "request token mismatch",
    )
    if not_before_ns is not None:
        require(
            all(
                isinstance(row.get("time_ns"), int) and row["time_ns"] >= not_before_ns
                for row in requests
            ),
            "stale request evidence",
        )
    prefix = "ss_" + token + "_"

    def cookies(row):
        keys = {part.strip().split("=")[0] for part in row.get("cookie", "").split(";")}
        return {
            key[len(prefix) :] for key in keys if key.startswith(prefix)
        } & BASE_KEYS

    def matching(path, host=None, method=None, step=None, request_phase=None):
        return [
            row
            for row in requests
            if row.get("path") == path
            and (host is None or row.get("host") == host)
            and (method is None or row.get("method") == method)
            and (step is None or row.get("query", {}).get("step") == step)
            and (
                request_phase is None
                or row.get("query", {}).get("phase") == request_phase
            )
        ]

    def check_wire(path, host, expected, **criteria):
        rows = matching(path, host, **criteria)
        require(bool(rows), "missing wire evidence: " + path + " " + str(criteria))
        for row in rows:
            require(row.get("tls") is True, "cookie observation was not HTTPS/WSS")
            require(
                cookies(row) == expected,
                "wrong wire cookies: "
                + path
                + " "
                + str(criteria)
                + ": "
                + str(cookies(row)),
            )

    require(
        len(matching("/start", a.netloc)) == 1
        and len(matching("/seed-b", b.netloc)) == 1,
        "start/seed must happen exactly once",
    )
    check_wire("/echo", other.netloc, ALL, request_phase="main")
    check_wire("/ws", a.netloc, ALL, request_phase="main")
    check_wire("/ws", b.netloc, {"none"}, request_phase="main")
    check_wire("/ws", a.netloc, {"none"}, request_phase="nested-child")
    check_wire("/nav", b.netloc, LAX, method="GET", step="get")
    check_wire("/nav", a.netloc, {"none"}, method="POST", step="post")
    check_wire("/nav", b.netloc, {"none"}, method="POST", step="post307")
    check_wire("/nav", a.netloc, LAX, method="GET", step="chain-end")
    check_wire("/nav", a.netloc, {"none"}, method="POST", step="scheme-post")
    check_wire("/nav", a.netloc, LAX, method="GET", step="scheme-get")
    require(
        matching("/nav", http_a.netloc, step="scheme-post-source")
        and matching("/nav", http_a.netloc, step="scheme-get-source"),
        "no HTTP initiating documents",
    )
    if phase == "resumed":
        require(
            len(matching("/resume", a.netloc)) == 1, "resume must happen exactly once"
        )
        check_wire("/resume", a.netloc, ALL - {"session"}, method="GET")
        check_wire("/resume-b", b.netloc, LAX, method="GET")
        require(
            not any(
                row["name"] in RESUME_NAMES
                and row["phase"] not in ("resume", "resume-b")
                for row in results
            ),
            "restart stage mismatch",
        )
    else:
        require(not matching("/resume"), "initial report already contains restart")
    return {
        "suite": "cookie-samesite",
        "run": token,
        "phase": phase,
        "passed": len(names),
        "total": len(names),
        "wire_requests": len(requests),
        "first_request_ns": min(row["time_ns"] for row in requests),
        "last_request_ns": max(row["time_ns"] for row in requests),
    }


def validate_initial(report, token, not_before_ns=None):
    return validate_report(report, token, "initial", not_before_ns)


def validate_resumed(report, token, not_before_ns=None):
    return validate_report(report, token, "resumed", not_before_ns)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--run", required=True)
    parser.add_argument("--phase", choices=("initial", "resumed"), default="initial")
    parser.add_argument("--not-before-ns", type=int)
    args = parser.parse_args()
    print(
        json.dumps(
            validate_report(args.report, args.run, args.phase, args.not_before_ns),
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
