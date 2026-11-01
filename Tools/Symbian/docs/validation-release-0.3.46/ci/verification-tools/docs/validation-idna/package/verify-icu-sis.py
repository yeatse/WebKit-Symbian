#!/usr/bin/env python3
"""Inspect one ICU data SIS, verify its checksums, and optionally extract its data."""
import argparse
import binascii
import hashlib
import json
import struct
import zlib
from pathlib import Path


def field(data, position=0):
    kind, length = struct.unpack_from("<II", data, position)
    end = position + 8 + length
    padded = position + 8 + ((length + 3) & ~3)
    if length >= 0x80000000 or padded > len(data):
        raise ValueError("Truncated or unsupported SIS field")
    return kind, data[position + 8:end], padded


def fields(data):
    position = 0
    result = []
    while position < len(data):
        start = position
        kind, payload, position = field(data, position)
        result.append((kind, payload, data[start:position]))
    return result


def array(data, expected_kind):
    kind, payload, end = field(data)
    if kind != 2 or end > len(data) or struct.unpack_from("<I", payload)[0] != expected_kind:
        raise ValueError("Unexpected SIS array")
    position = 4
    result = []
    while position < len(payload):
        length = struct.unpack_from("<I", payload, position)[0]
        end = position + 4 + length
        padded = position + 4 + ((length + 3) & ~3)
        if padded > len(payload):
            raise ValueError("Truncated SIS array item")
        result.append(payload[position + 4:end])
        position = padded
    return result


def decompress(payload):
    algorithm, size = struct.unpack_from("<IQ", payload)
    if algorithm not in (0, 1):
        raise ValueError("Unsupported SIS compression")
    data = zlib.decompress(payload[12:]) if algorithm else payload[12:]
    if len(data) != size:
        raise ValueError("SIS uncompressed size mismatch")
    return data


def inspect(path):
    image = path.read_bytes()
    uid1, uid2, uid3, checksum = struct.unpack_from("<IIII", image)
    actual = binascii.crc_hqx(image[:12:2], 0) | (binascii.crc_hqx(image[1:12:2], 0) << 16)
    if (uid1, uid2, uid3) != (0x10201A7A, 0, 0xE1A5C013) or checksum != actual:
        raise ValueError("Unexpected ICU SIS UID/header checksum")
    kind, contents, end = field(image, 16)
    if kind != 12 or end != len(image):
        raise ValueError("Unexpected SISContents")
    parts = {kind: (payload, raw) for kind, payload, raw in fields(contents)}
    for checksum_kind, payload_kind in ((34, 3), (35, 30)):
        if struct.unpack("<H", parts[checksum_kind][0])[0] != binascii.crc_hqx(parts[payload_kind][1], 0):
            raise ValueError("SIS section CRC mismatch")
    kind, controller, _ = field(decompress(parts[3][0]))
    if kind != 13:
        raise ValueError("Missing SISController")
    children = fields(controller)
    sections = {kind: payload for kind, payload, _ in children}
    info = {kind: payload for kind, payload, _ in fields(sections[14][:-2])}
    if struct.unpack("<I", info[9])[0] != uid3:
        raise ValueError("Controller UID mismatch")
    version = ".".join(map(str, struct.unpack("<III", info[4])))
    descriptions = array(sections[28], 24)
    if len(descriptions) != 1:
        raise ValueError("ICU SIS must install exactly one data file")
    description = descriptions[0]
    kind, target, position = field(description)
    if kind != 1:
        raise ValueError("Missing install path")
    target = target.decode("utf-16le")
    if target.lower() != "c:\\resource\\icu\\icudt70l.dat":
        raise ValueError("Unexpected ICU install path: " + target)
    kind, _, position = field(description, position)  # MIME type.
    if kind != 1:
        raise ValueError("Missing MIME field")
    kind, hash_payload, position = field(description, position)
    if kind != 25 or struct.unpack_from("<I", hash_payload)[0] != 1:
        raise ValueError("Missing SHA-1 file hash")
    kind, expected_hash, _ = field(hash_payload, 4)
    if kind != 37:
        raise ValueError("Missing hash blob")
    units = array(parts[30][0], 31)
    if len(units) != 1:
        raise ValueError("Unexpected data units")
    items = array(units[0], 32)
    if len(items) != 1:
        raise ValueError("Unexpected data items")
    kind, compressed, _ = field(items[0])
    if kind != 3:
        raise ValueError("Missing compressed data")
    data = decompress(compressed)
    if hashlib.sha1(data).digest() != expected_hash:
        raise ValueError("File payload hash mismatch")
    return {
        "path": str(path), "bytes": len(image),
        "sha256": hashlib.sha256(image).hexdigest(), "uid3": hex(uid3),
        "version": version, "target": target,
        "signature_chains": sum(kind == 39 for kind, _, _ in children),
        "payload_bytes": len(data), "payload_sha256": hashlib.sha256(data).hexdigest(),
        "crc_and_payload_hash_valid": True,
    }, data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sis", type=Path)
    parser.add_argument("--version")
    parser.add_argument("--extract", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report, data = inspect(args.sis)
    if args.version and report["version"] != args.version:
        raise ValueError("Package version differs from the expected CI/local version")
    if args.extract:
        args.extract.parent.mkdir(parents=True, exist_ok=True)
        args.extract.write_bytes(data)
    output = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.write_text(output)
    print(output, end="")


if __name__ == "__main__":
    main()
