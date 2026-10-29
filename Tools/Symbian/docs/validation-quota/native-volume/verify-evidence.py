#!/usr/bin/env python3
"""Verify this portable record; does not build or run an emulator."""

import gzip
import hashlib
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parent


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_json(name):
    return json.loads((ROOT / name).read_text())


def main():
    manifest = read_json("archive-manifest.json")
    actual_files = {str(p.relative_to(ROOT)) for p in ROOT.rglob("*")
                    if p.is_file() and p.name != "archive-manifest.json"
                    and "__pycache__" not in p.parts}
    require(actual_files == set(manifest["files"]), "archive file set differs")
    for name, expected in manifest["files"].items():
        data = (ROOT / name).read_bytes()
        require(len(data) == expected["bytes"], f"size: {name}")
        require(hashlib.sha256(data).hexdigest() == expected["sha256"], f"SHA-256: {name}")
    raw = {}
    for name, expected in read_json("raw-index.json").items():
        data = (ROOT / name).read_bytes()
        if expected["encoding"] == "gzip":
            data = gzip.decompress(data)
        else:
            require(expected["encoding"] == "identity", f"unknown encoding: {name}")
        require(len(data) == expected["raw_bytes"], f"raw size: {name}")
        require(hashlib.sha256(data).hexdigest() == expected["raw_sha256"], f"raw SHA-256: {name}")
        raw[name] = data

    old, new = (read_json(f"{label}/result.json") for label in ("old-host", "new-host"))
    expected_hosts = [
        "12c275e7f7b3fa8da4fa58ef4d048799db278baaeca7a45d3f78debad3051a4c",
        "5f8704eb1c2461ceeb160eabd927a4f828765cad353e9107ae43f463127e292a",
    ]
    for label, result, host, count, failures, guest_exit in (
        ("old-host", old, expected_hosts[0], 6, 4, 1),
        ("new-host", new, expected_hosts[1], 10, 0, 0),
    ):
        require(result["emulator_sha256"] == host, f"host identity: {label}")
        require(result["host_exit"] == 0 and result["guest_exit"] == guest_exit,
                f"exit status: {label}")
        require(result["panic_or_access_violation"] is False, f"panic: {label}")
        require(result["result"] == {"probe": "native-volume", "passed": count, "failed": failures},
                f"counts: {label}")
        require(result["status"] == ("FAIL" if failures else "PASS"), f"status: {label}")
        lines = raw[f"{label}/volume-probe.log.gz"].decode().splitlines()
        require(sum(line.startswith("PASS ") for line in lines) == count, f"PASS lines: {label}")
        require(sum(line.startswith("FAIL ") for line in lines) == failures, f"FAIL lines: {label}")
        markers = [line for line in lines if line.startswith("RESULT ")]
        require(len(markers) == 1 and json.loads(markers[0][7:]) == result["result"],
                f"unique guest result: {label}")

    identity = read_json("identities.json")
    require(old["image_sha256"] == new["image_sha256"] == identity["probe_image"]["sha256"],
            "same probe identity")
    deployment = read_json("host-0018-identities.json")
    require(old["emulator_patch_assets"] == new["emulator_patch_assets"] == deployment["native_assets"],
            "native DLL/map deployment identities")
    require(deployment["host"]["sha256"] == new["emulator_sha256"], "rebuilt host identity")
    require(identity["e32_header"]["capabilities"] == "0x0", "probe capability grants")
    require(identity["e32_header"]["floating_point_type"] == 0, "probe floating-point type")
    for name, expected in identity["source_files"].items():
        data = (ROOT / "source-snapshot" / name).read_bytes()
        require(hashlib.sha256(data).hexdigest() == expected["sha256"], f"source snapshot: {name}")

    log = raw["new-host/volume-probe.log.gz"].decode()
    volumes = re.findall(r"^VOLUME ([CEZ]) capacity=(\d+) available=(\d+) native-error=(\d+)$", log, re.M)
    require(len(volumes) == 3, "three mounted volumes")
    for drive, capacity, available, error in volumes:
        require(int(capacity) == 33770192896 and int(error) == 0, f"64-bit volume: {drive}")
        require(int(available) == (0 if drive == "Z" else 6952808448), f"available: {drive}")
    matches = re.findall(r"^VOLUME_WRITE before=(\d+) after=(\d+) error=(\d+)$", log, re.M)
    require(matches == [("6952808448", "6950703104", "0")], "write measurements")
    require(int(matches[0][0]) - int(matches[0][1]) == 2105344, "write delta")
    host = read_json("host-filesystem-comparison.json")
    require(host["driver_exit"] == 0, "host comparison driver exit")
    for phase in ("before", "after"):
        for drive in "cez":
            sample = host[phase]["drives"][drive]
            require(sample["capacity"] == 33770192896, "host capacity")
            require(sample["available"] < sample["free"] <= sample["capacity"], "reserved host space")
    require(raw["new-host/previous-volume-probe.log.gz"] == raw["old-host/volume-probe.log.gz"],
            "retained previous log remains the old failure")
    print("PASS: archive hashes and lossless originals; old 6/10 FAIL and new 10/10 PASS; same probe/native assets; 64-bit capacity and write delta")


if __name__ == "__main__":
    main()
