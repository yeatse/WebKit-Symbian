#!/usr/bin/env python3
"""Verify the complete, uniquely identified RSA browser log after strict launch."""
import argparse
import json
from pathlib import Path
import re

EXTRA = [
    "generateKey metadata usages extractability and asynchronous completion",
    "all three algorithms support JWK SPKI PKCS8 roundtrips",
    "minimal private JWK works and malformed CRT or partial fields are rejected",
    "DER rejects trailing truncated noncanonical and wrong formats",
    "PSS exact salt bounds and unsigned overflow",
    "OAEP empty maximum label mismatch ciphertext checks and wrap unwrap",
    "usage extractability hash and key-type rejection",
    "parallel operations on one private key keep independent state",
    "DedicatedWorker imports and performs actual RSA operations",
]

def verify(log, token, vectors):
    if re.search(r"RSA_FATAL|RSA_TIMEOUT|access violation|panicked|KERN-EXEC|USER-EXEC|segmentation fault", log, re.I):
        raise ValueError("fatal, timeout or native failure in fresh guest log")
    records = re.findall(r"RSA_RESULT (\{[^\r\n]+\})", log)
    if len(records) != 1:
        raise ValueError("expected one final RSA report")
    report = json.loads(records[0])
    expected = ["known answer " + vector["name"] for vector in vectors] + EXTRA
    if report.get("run") != token or report.get("total") != len(expected) or report.get("passed") != len(expected):
        raise ValueError("wrong run identity or totals")
    rows = report.get("results", [])
    if [row.get("name") for row in rows] != expected or any(row.get("pass") is not True or row.get("error") is not None for row in rows):
        raise ValueError("missing, reordered or failed case")
    elapsed = report.get("elapsed_ms")
    if type(elapsed) not in (int, float) or not 0 <= elapsed <= 180000:
        raise ValueError("invalid or exceeded page deadline")
    for name in expected:
        marker = "RSA_CASE run=" + token + " PASS " + name
        if log.count(marker) != 1:
            raise ValueError("missing or repeated case console marker")
    if log.count(f"RSA_DONE run={token} passed={len(expected)} total={len(expected)}") != 1:
        raise ValueError("missing or repeated terminal marker")
    return report

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--run", required=True)
    parser.add_argument("--vectors", type=Path, required=True)
    args = parser.parse_args()
    report = verify(args.log.read_text(errors="replace"), args.run, json.loads(args.vectors.read_text()))
    print(json.dumps(report, indent=2))

if __name__ == "__main__": main()
