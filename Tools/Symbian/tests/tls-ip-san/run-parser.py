#!/usr/bin/env python3
"""Compare the actual SDK internal parser with host inet_pton on fixed inputs."""

import argparse
import hashlib
import ipaddress
import json
import random
import subprocess
from pathlib import Path


def cases():
    values = [
        "",
        "::",
        "::: ",
        "::: ".strip(),
        "::1",
        "1::",
        "1::2::3",
        "[::1]",
        "::1%lo",
        "1:2:3:4:5:6:7::",
        "1:2:3:4:5:6:192.0.2.1",
        "1:2:3:4:5:6:7:8:",
        "1:2:3:4:5:6:7:8",
        "1:2:3:4:5:6:7:8:9",
        "127.0.0.1",
        "127.00.0.1",
        "256.0.0.1",
        "255.255.255.256",
        "1.2.3.4.5",
        "1.2.3",
        ".1.2.3.4",
        "-1.2.3.4",
    ]
    for start in range(8):
        for count in range(1, 9 - start):
            groups = [index + 1 for index in range(8)]
            groups[start : start + count] = [0] * count
            address = ipaddress.IPv6Address(
                ":".join(format(group, "x") for group in groups)
            )
            values.extend(
                [
                    address.compressed,
                    address.exploded,
                    address.exploded + ":",
                    ":" + address.exploded,
                ]
            )
    for count in range(8):
        prefix = ":".join(str(index + 1) for index in range(count))
        values.extend([prefix + ":192.0.2.1", prefix + "::192.0.2.1"])
    generator = random.Random(20261029)
    for _ in range(256):
        address = ipaddress.IPv6Address(generator.getrandbits(128))
        values.extend(
            [
                str(address),
                address.exploded,
                str(address) + ":",
                ":" + str(address),
                str(address) + "x",
            ]
        )
        ipv4 = str(ipaddress.IPv4Address(generator.getrandbits(32)))
        values.extend([ipv4, ipv4 + ".", ipv4 + "x"])
    return list(dict.fromkeys(values))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    inputs = cases()
    data = "\n".join(inputs) + "\n"
    (args.output / "inputs.txt").write_text(data)
    run = subprocess.run(
        [str(args.probe.resolve())],
        input=data,
        capture_output=True,
        text=True,
        check=False,
        timeout=15,
    )
    (args.output / "log.txt").write_text(run.stdout + run.stderr)
    expected_summary = f"PARSER_RESULT {len(inputs)} cases 0 mismatches"
    passed = run.returncode == 0 and run.stdout.splitlines() == [expected_summary]
    summary = {
        "status": "PASS" if passed else "FAIL",
        "cases": len(inputs),
        "exit_code": run.returncode,
        "seed": 20261029,
        "probe_sha256": hashlib.sha256(args.probe.read_bytes()).hexdigest(),
        "inputs_sha256": hashlib.sha256(data.encode()).hexdigest(),
    }
    (args.output / "result.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(run.stdout, end="")
    return int(not passed)


if __name__ == "__main__":
    raise SystemExit(main())
