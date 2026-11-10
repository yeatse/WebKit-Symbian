"""Exact page-result contract; a feature-presence result is never sufficient."""
import re

CASES = {
    "write": (
        "localStorage string operations",
        "localStorage quota rejection is atomic",
        "sessionStorage string operations",
        "same origin localStorage sharing",
        "different origin localStorage isolation",
        "sessionStorage origin isolation",
        "storage event and no self notification",
        "BroadcastChannel structured clone and no self echo",
        "BroadcastChannel rejects uncloneable values",
        "BroadcastChannel same origin iframe transport",
        "BroadcastChannel different origin isolation",
        "BroadcastChannel close suppresses queued delivery",
        "DedicatedWorker transfer and storage exposure",
        "BroadcastChannel document worker roundtrip",
        "BroadcastChannel worker close stops messages",
        "persistent value staged for restart",
    ),
    "read": (
        "localStorage survives browser restart",
        "sessionStorage does not survive browser restart",
        "persistent removal",
    ),
    "erased": ("removal survives browser restart",),
}

BACKEND_CASES = {
    "write": (
        "IndexedDB schema and committed records",
        "IndexedDB index and cursor ordering",
        "IndexedDB explicit abort preserves data",
        "IndexedDB constraint abort preserves data",
        "IndexedDB close and reopen",
        "IndexedDB versionchange and blocked handoff",
        "IndexedDB DedicatedWorker read write",
        "IndexedDB origin isolation",
        "Web Locks exclusive ordering",
        "Web Locks shared holders and exclusive waiter",
        "Web Locks ifAvailable and abort",
        "Web Locks DedicatedWorker lifetime",
        "Web Locks origin isolation",
        "IndexedDB persistent value staged",
    ),
    "read": ("IndexedDB survives browser restart", "IndexedDB persistent deletion"),
    "erased": ("IndexedDB deletion survives browser restart",),
}

SUITES = {"storage": CASES, "backends": BACKEND_CASES}


def identity(report, suite="storage"):
    if not isinstance(report, dict) or report.get("suite") != "belle-" + suite:
        raise ValueError("wrong suite")
    token, phase = report.get("run"), report.get("phase")
    if not isinstance(token, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,80}", token):
        raise ValueError("invalid run token")
    if phase not in CASES:
        raise ValueError("invalid phase")
    return token, phase


def validate(report, token, phase, origin, suite="storage"):
    if identity(report, suite) != (token, phase) or report.get("origin") != origin:
        raise ValueError("wrong run, phase, or origin")
    rows = report.get("cases")
    if not isinstance(rows, list) or [row.get("name") for row in rows if isinstance(row, dict)] != list(SUITES[suite][phase]):
        raise ValueError("missing, extra, or reordered outcomes")
    if report.get("status") != "PASS" or any(row.get("status") != "PASS" for row in rows):
        raise ValueError("page reported a failure")
    elapsed = report.get("elapsed_ms")
    deadline = 45000 if suite == "storage" else 55000
    if type(elapsed) not in (int, float) or not 0 <= elapsed <= deadline:
        raise ValueError("invalid page deadline")
