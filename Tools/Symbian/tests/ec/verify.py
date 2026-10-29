#!/usr/bin/env python3
"""Verify the complete, uniquely identified EC browser log after strict launch."""
import argparse
import json
from pathlib import Path
import re

EXTRA = ["usage and key type rejection", "curve and algorithm rejection",
         "DER format length and curve rejection", "unsupported curve and hash rejection",
         "same-key parallel operations", "DedicatedWorker EC operations"]

def expected_names(vectors):
    result = ["known ECDSA " + vector["name"] for vector in vectors["ecdsa"]]
    result += ["known ECDH " + vector["curve"] for vector in vectors["ecdh"]]
    for curve in ("P-256", "P-384", "P-521"):
        result += [curve + " " + suffix for suffix in ("key formats", "JWK validation", "generation metadata")]
    return result + EXTRA

def verify(log, token, vectors):
    if re.search(r"EC_FATAL|EC_TIMEOUT|access violation|panicked|KERN-EXEC|USER-EXEC|segmentation fault", log, re.I):
        raise ValueError("fatal, timeout or native failure in fresh guest log")
    reports = re.findall(r"EC_RESULT (\{[^\r\n]+\})", log)
    if len(reports) != 1:
        raise ValueError("expected one final EC report")
    report = json.loads(reports[0])
    expected = expected_names(vectors)
    if report.get("run") != token or report.get("passed") != len(expected) or report.get("total") != len(expected):
        raise ValueError("wrong run identity or totals")
    rows = report.get("results", [])
    if [row.get("name") for row in rows] != expected or any(row.get("pass") is not True or row.get("error") is not None for row in rows):
        raise ValueError("missing, reordered or failed case")
    elapsed = report.get("elapsed_ms")
    if type(elapsed) not in (int, float) or not 0 <= elapsed <= 180000:
        raise ValueError("invalid or exceeded page deadline")
    for name in expected:
        if log.count("EC_CASE run=" + token + " PASS " + name) != 1:
            raise ValueError("missing or repeated case marker")
    if log.count(f"EC_DONE run={token} passed={len(expected)} total={len(expected)}") != 1:
        raise ValueError("missing or repeated terminal marker")
    return report

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--run", required=True)
    parser.add_argument("--vectors", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.log.read_text(errors="replace"), args.run, json.loads(args.vectors.read_text())), indent=2))
