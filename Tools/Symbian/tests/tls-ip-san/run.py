#!/usr/bin/env python3
"""Verify signed certificates and actual in-memory TLS ClientHello records."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def server_names(data):
    handshake = bytearray()
    position = 0
    while position < len(data):
        if len(data) - position < 5:
            raise ValueError("truncated TLS record")
        kind, length = (
            data[position],
            int.from_bytes(data[position + 3 : position + 5], "big"),
        )
        position += 5
        if position + length > len(data) or kind != 22:
            raise ValueError("unexpected/truncated first-flight TLS record")
        handshake.extend(data[position : position + length])
        position += length
    if (
        len(handshake) < 4
        or handshake[0] != 1
        or int.from_bytes(handshake[1:4], "big") != len(handshake) - 4
    ):
        raise ValueError("not one complete ClientHello")
    position = 4 + 2 + 32

    def take(length):
        nonlocal position
        if position + length > len(handshake):
            raise ValueError("truncated ClientHello field")
        result = handshake[position : position + length]
        position += length
        return result

    take(int.from_bytes(take(1), "big"))
    take(int.from_bytes(take(2), "big"))
    take(int.from_bytes(take(1), "big"))
    extension_length = int.from_bytes(take(2), "big")
    if position + extension_length != len(handshake):
        raise ValueError("ClientHello extension length mismatch")
    names = []
    seen = set()
    while position < len(handshake):
        kind = int.from_bytes(take(2), "big")
        payload = take(int.from_bytes(take(2), "big"))
        if kind in seen:
            raise ValueError("duplicate ClientHello extension")
        seen.add(kind)
        if kind != 0:
            continue
        if len(payload) < 2 or int.from_bytes(payload[:2], "big") != len(payload) - 2:
            raise ValueError("invalid server_name extension length")
        offset = 2
        while offset < len(payload):
            if len(payload) - offset < 3 or payload[offset] != 0:
                raise ValueError("invalid server_name entry")
            length = int.from_bytes(payload[offset + 1 : offset + 3], "big")
            offset += 3
            if not length or offset + length > len(payload):
                raise ValueError("invalid server_name value")
            names.append(bytes(payload[offset : offset + length]).decode("ascii"))
            offset += length
    return names


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    probe = args.probe.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    manifest = json.loads((args.fixtures / "manifest.json").read_text())
    for name, expected in manifest["files"].items():
        if hashlib.sha256((args.fixtures / name).read_bytes()).hexdigest() != expected:
            raise ValueError("certificate fixture changed: " + name)
    results = []
    for case in manifest["cases"]:
        run = subprocess.run(
            [
                str(probe),
                str(args.fixtures / case["certificate"]),
                str(args.fixtures / "ca.der"),
                case["hostname"],
            ],
            capture_output=True,
            text=True,
            check=False,
            timeout=10,
        )
        actual = json.loads(run.stdout)
        expected_flags = case["required_flags"]
        success = (
            run.returncode == 0
            and actual["parse_result"] == 0
            and actual["flags"] == expected_flags
            and (actual["verify_result"] == 0) == (expected_flags == 0)
        )
        results.append(
            {**case, "actual": actual, "status": "PASS" if success else "FAIL"}
        )
        print(results[-1]["status"], case["certificate"], case["hostname"], actual)
    for version, mode in (("TLS1.2", "--hello"), ("TLS1.3", "--hello13")):
        for number, hostname in enumerate(
            ["localhost", "www.example.test", "127.0.0.1", "::1", "2001:db8::1"]
        ):
            path = args.output / f"client-hello-{version}-{number}.bin"
            run = subprocess.run(
                [str(probe), mode, hostname, str(path)],
                capture_output=True,
                text=True,
                check=False,
                timeout=10,
            )
            actual = json.loads(run.stdout)
            names = server_names(path.read_bytes()) if run.returncode == 0 else None
            expected = [hostname] if number < 2 else []
            success = (
                run.returncode == 0 and actual["result"] == 0 and names == expected
            )
            results.append(
                {
                    "client_hello_hostname": hostname,
                    "tls_version": version,
                    "server_names": names,
                    "actual": actual,
                    "status": "PASS" if success else "FAIL",
                }
            )
            print(results[-1]["status"], "ClientHello", version, hostname, names)
    summary = {
        "status": "PASS"
        if all(item["status"] == "PASS" for item in results)
        else "FAIL",
        "passed": sum(item["status"] == "PASS" for item in results),
        "total": len(results),
        "probe_sha256": hashlib.sha256(probe.read_bytes()).hexdigest(),
        "fixture_manifest_sha256": hashlib.sha256(
            (args.fixtures / "manifest.json").read_bytes()
        ).hexdigest(),
        "results": results,
    }
    (args.output / "result.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"RESULT {summary['passed']}/{summary['total']} {summary['status']}")
    return int(summary["status"] != "PASS")


if __name__ == "__main__":
    raise SystemExit(main())
