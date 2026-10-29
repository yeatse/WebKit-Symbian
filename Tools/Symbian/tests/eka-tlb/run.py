#!/usr/bin/env python3
"""Run compiled real-JIT probes in separate processes; preserve crash controls."""

import argparse
import json
import resource
import subprocess
from pathlib import Path

CASES = (
    "cold-read",
    "cold-write",
    "cold-byte-read",
    "cold-byte-write",
    "cold-half-read",
    "cold-half-write",
    "cold-thumb-read",
    "cold-thumb-write",
    "flush-read",
    "flush-write",
    "dirty-read",
    "dirty-write",
    "null-base-read",
    "null-base-write",
    "null-base-highest-read",
    "denied-read",
    "denied-write",
    "collision-denied-read",
    "collision-denied-write",
    "valid-read",
    "valid-write",
    "valid-byte-read",
    "valid-byte-write",
    "valid-half-read",
    "valid-half-write",
    "valid-thumb-read",
    "valid-thumb-write",
    "highest-read",
    "highest-write",
    "valid-highest-read",
    "valid-highest-write",
    "highest-denied-read",
    "highest-denied-write",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--old", type=Path, required=True)
    parser.add_argument("--new", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    results = {}
    binaries = [("old", args.old), ("corrected", args.new)]
    for phase, binary in binaries:
        rows = []
        for case in CASES:
            result = subprocess.run(
                [str(binary.resolve()), case],
                capture_output=True,
                text=True,
                timeout=10,
                check=False,
            )
            if phase.startswith("corrected") or case.startswith(("valid-", "highest-")):
                expected = 0
            elif "denied" in case:
                expected = 1
            else:
                expected = -11
            rows.append(
                {
                    "case": case,
                    "returncode": result.returncode,
                    "expected": expected,
                    "stdout": result.stdout,
                    "stderr": result.stderr,
                }
            )
        results[phase] = rows
    passed = all(
        row["returncode"] == row["expected"]
        for rows in results.values()
        for row in rows
    )
    result = {"passed": passed, "results": results}
    (args.output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    for phase, rows in results.items():
        print(
            f"{phase}: {sum(row['returncode'] == row['expected'] for row in rows)}/{len(rows)} expected outcomes"
        )
        for row in rows:
            if row["returncode"] != row["expected"]:
                print(row)
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
