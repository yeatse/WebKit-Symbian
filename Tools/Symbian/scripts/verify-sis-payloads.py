#!/usr/bin/env python3
"""Verify local SIS identities, CRCs and every payload against a pinned source plan.

This is artifact verification only. It neither installs a package nor proves
that its payloads run on a device. Existing release verification scripts remain
unchanged; this command accepts the package/version/source plan explicitly.
"""

import argparse
import binascii
import hashlib
import importlib.util
import json
import struct
from pathlib import Path

PARSER = (
    Path(__file__).resolve().parents[1]
    / "docs/validation-idna/package/verify-icu-sis.py"
)
spec = importlib.util.spec_from_file_location("sis_fields", PARSER)
sis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sis)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def uid_checksum(data):
    return binascii.crc_hqx(data[:12:2], 0) | (binascii.crc_hqx(data[1:12:2], 0) << 16)


def unique_field(entries, kind):
    matches = [(payload, raw) for tag, payload, raw in entries if tag == kind]
    require(len(matches) == 1, f"expected exactly one SIS field {kind}")
    return matches[0]


def e32_info(payload, expected, declared_capabilities):
    require(len(payload) >= 156 and payload[16:20] == b"EPOC", "missing E32 header")
    word = lambda offset: struct.unpack_from("<I", payload, offset)[0]
    require((word(0), word(4)) == (0x1000007A, 0), "expected E32 executable")
    require(
        word(8) == int(expected["uid3"], 0) and word(128) == word(8),
        "E32 UID/SID mismatch",
    )
    require(word(12) == uid_checksum(payload), "E32 UID checksum mismatch")
    size = word(100)
    require(156 <= size <= len(payload), "invalid E32 header length")
    header = bytearray(payload[:size])
    struct.pack_into("<I", header, 20, 0xC90FDAA2)
    crc = 0
    for byte in header:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    require(word(20) == crc, "E32 header CRC mismatch")
    capabilities = struct.unpack_from("<Q", payload, 136)[0]
    require(
        capabilities == declared_capabilities == int(expected["capabilities"], 0),
        "SIS/E32/planned capabilities differ",
    )
    float_type = (word(44) >> 20) & 15
    require(
        float_type == expected["hardware_float_type"],
        "unexpected E32 floating-point requirement",
    )
    return {
        "uid3": hex(word(8)),
        "secure_id": hex(word(128)),
        "flags": hex(word(44)),
        "capabilities": hex(capabilities),
        "hardware_float_type": float_type,
        "uid_checksum_valid": True,
        "header_crc_valid": True,
    }


def verify_package(path, plan, expected_inputs, expected_version, output):
    image = path.read_bytes()
    require(len(image) >= 16, "empty/truncated SIS")
    uid = int(plan["package_uids"][path.name], 0)
    require(
        struct.unpack_from("<IIII", image) == (0x10201A7A, 0, uid, uid_checksum(image)),
        "SIS UID/header checksum mismatch",
    )
    kind, contents, end = sis.field(image, 16)
    require(
        kind == 12 and end == len(image), "unexpected SISContents or trailing bytes"
    )
    parts = sis.fields(contents)
    require(
        sorted(tag for tag, _, _ in parts) == [3, 30, 34, 35],
        "unexpected top-level SIS fields",
    )
    for checksum_kind, payload_kind in ((34, 3), (35, 30)):
        expected_crc, _ = unique_field(parts, checksum_kind)
        _, raw = unique_field(parts, payload_kind)
        require(
            struct.unpack("<H", expected_crc)[0] == binascii.crc_hqx(raw, 0),
            "SIS section CRC mismatch",
        )
    compressed_controller, _ = unique_field(parts, 3)
    expanded = sis.decompress(compressed_controller)
    kind, controller, end = sis.field(expanded)
    require(kind == 13 and end == len(expanded), "unexpected SISController")
    children = sis.fields(controller)
    info, _ = unique_field(children, 14)
    require(info[-2:] == b"\0\0", "unexpected install type/flags")
    info_fields = sis.fields(info[:-2])
    controller_uid, _ = unique_field(info_fields, 9)
    require(struct.unpack("<I", controller_uid)[0] == uid, "controller UID mismatch")
    version_field, _ = unique_field(info_fields, 4)
    version = ".".join(map(str, struct.unpack("<III", version_field)))
    require(version == expected_version, "SIS version mismatch")
    signatures = sum(tag == 39 for tag, _, _ in children)
    require(
        signatures == plan["expected_signature_chains"],
        "unexpected signature-chain count",
    )
    install, _ = unique_field(children, 28)
    arrays = sis.fields(install)
    require(
        len(arrays) == 3 and all(tag == 2 for tag, _, _ in arrays),
        "unexpected install block",
    )
    descriptions = sis.array(arrays[0][2], 24)
    require(
        not sis.array(arrays[1][2], 13) and not sis.array(arrays[2][2], 26),
        "unexpected embedded controller or condition",
    )
    data, _ = unique_field(parts, 30)
    require(len(sis.fields(data)) == 1, "unexpected SISData fields")
    units = sis.array(data, 31)
    require(len(units) == 1 and len(sis.fields(units[0])) == 1, "unexpected data units")
    items = sis.array(units[0], 32)
    require(
        len(items) == len(descriptions) == len(expected_inputs),
        "payload count mismatch",
    )
    payloads = []
    for index, (description, source) in enumerate(zip(descriptions, expected_inputs)):
        kind, target_bytes, position = sis.field(description)
        target = target_bytes.decode("utf-16le")
        require(
            kind == 1 and target == source["destination"],
            "payload destination/order mismatch",
        )
        kind, mime, position = sis.field(description, position)
        require(kind == 1 and not mime, "unexpected MIME field")
        kind, value, position = sis.field(description, position)
        capabilities = 0
        if kind == 41:
            capabilities = struct.unpack("<I", value)[0]
            kind, value, position = sis.field(description, position)
        require(
            kind == 25 and struct.unpack_from("<I", value)[0] == 1,
            "missing SHA-1 file hash",
        )
        kind, expected_hash, end = sis.field(value, 4)
        require(
            kind == 37 and end == len(value) and len(expected_hash) == 20,
            "unexpected hash blob",
        )
        operation, options, stored, size, file_index = struct.unpack(
            "<IIQQI", description[position:]
        )
        require(
            operation == 1 and not options and file_index == index,
            "unexpected install operation or data index",
        )
        kind, compressed, end = sis.field(items[index])
        require(kind == 3 and end == len(items[index]), "unexpected file data")
        payload = sis.decompress(compressed)
        require(
            len(payload) == size and len(compressed) - 12 == stored,
            "payload size mismatch",
        )
        require(
            hashlib.sha1(payload).digest() == expected_hash, "SIS file hash mismatch"
        )
        source_path = Path(source["source"])
        source_bytes = source_path.read_bytes()
        require(
            len(source_bytes) == source["bytes"]
            and sha256(source_bytes) == source["sha256"],
            "source differs from pinned input plan",
        )
        require(payload == source_bytes, "SIS payload differs from source bytes")
        entry = {
            "target": target,
            "source": str(source_path),
            "bytes": len(payload),
            "sha256": sha256(payload),
            "sis_sha1_valid": True,
            "source_bytes_equal": True,
        }
        if "e32" in source:
            entry["e32"] = e32_info(payload, source["e32"], capabilities)
        else:
            require(
                capabilities == 0, "non-executable unexpectedly declares capabilities"
            )
        extracted = output / "payloads" / path.stem / target.rsplit("\\", 1)[-1]
        extracted.parent.mkdir(parents=True, exist_ok=True)
        require(
            not extracted.exists() or extracted.read_bytes() == payload,
            "existing extraction differs",
        )
        extracted.write_bytes(payload)
        entry["extracted_path"] = str(extracted)
        payloads.append(entry)
    return {
        "package": path.name,
        "path": str(path),
        "bytes": len(image),
        "sha256": sha256(image),
        "uid3": hex(uid),
        "version": version,
        "signature_chains": signatures,
        "signature_status": "unsigned"
        if not signatures
        else "present; cryptographic validation not performed",
        "sis_header_checksum_valid": True,
        "controller_and_data_crc_valid": True,
        "payloads": payloads,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sis-dir", required=True, type=Path)
    parser.add_argument("--plan", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    report = {
        "status": "FAIL",
        "scope": "local package verification only; not installed or guest-tested",
        "expected_version": args.version,
    }
    try:
        plan_bytes = args.plan.read_bytes()
        plan = json.loads(plan_bytes)
        require(plan["sis_version"] == args.version, "plan/version mismatch")
        require(
            set(plan["packages"]) == set(plan["package_uids"]),
            "inconsistent package plan",
        )
        names = {path.name for path in args.sis_dir.glob("*.sis")}
        require(names == set(plan["packages"]), "unexpected or missing SIS package")
        report["plan"] = {
            "path": str(args.plan.resolve()),
            "sha256": sha256(plan_bytes),
        }
        report["validator_sha256"] = sha256(Path(__file__).read_bytes())
        report["field_parser_sha256"] = sha256(PARSER.read_bytes())
        packages = []
        for name, count in plan["packages"].items():
            inputs = [item for item in plan["inputs"] if item["package"] == name]
            require(len(inputs) == count, "input plan package count mismatch")
            packages.append(
                verify_package(args.sis_dir / name, plan, inputs, args.version, output)
            )
        payload_count = sum(len(package["payloads"]) for package in packages)
        require(
            payload_count == plan["payload_count"] == len(plan["inputs"]),
            "total payload count mismatch",
        )
        report.update(
            status="PASS",
            sis_version=args.version,
            payload_count=payload_count,
            packages=packages,
        )
        (output / "SHA256SUMS").write_text(
            "".join(
                f"{package['sha256']}  {package['package']}\n" for package in packages
            )
        )
    except (ValueError, OSError, struct.error, KeyError, TypeError) as error:
        report["error"] = str(error)
    (output / "package-verification.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(json.dumps({key: report[key] for key in ("status", "expected_version")}))
    if report["status"] != "PASS":
        raise SystemExit(report.get("error", "package verification failed"))


if __name__ == "__main__":
    main()
