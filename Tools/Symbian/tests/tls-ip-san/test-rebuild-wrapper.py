#!/usr/bin/env python3
"""Exercise input rejection with real pinned SDK bytes in temporary copies."""

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/rebuild-sdk-mbedtls-ip.py"
MANIFEST = ROOT / "patches/sdk-0.3.0/mbedtls-ip/manifest.json"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def replace(path, content):
    if path.exists() or path.is_symlink():
        path.unlink()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sdk, work = args.sdk.resolve(), args.work_dir.resolve()
    manifest = json.loads(MANIFEST.read_text())
    for name, record in manifest["archives"].items():
        for directory, key in [("base", "stock_sha256"), ("lib", "patched_sha256")]:
            if digest(work / directory / name) != record[key]:
                raise SystemExit("Expected verified original and rebuilt archives")
    cases = [
        ("compiler", "Unknown SDK compiler"),
        ("header", "SDK header mismatch"),
        ("crypto", "Unknown crypto archive"),
        ("x509", "Unknown installed archive: libmbedx509.a"),
        ("tls", "Unknown installed archive: libmbedtls.a"),
        ("backup", "Original archive backup changed"),
        ("repaired-without-backup", "Original archive backup required"),
        ("source", "SDK source mismatch: CMakeLists.txt"),
    ]
    results = []
    with tempfile.TemporaryDirectory(prefix="mbedtls-wrapper-") as temporary:
        for label, expected in cases:
            case = Path(temporary) / label
            copied_sdk, copied_work = case / "sdk", case / "work"
            library = copied_sdk / "lib/armv6/mbedtls/lib"
            library.mkdir(parents=True)
            for relative in ["bin/clang", *manifest["sdk_headers"]]:
                target = copied_sdk / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.symlink_to(sdk / relative)
            crypto = "libmbedcrypto.a"
            (library / crypto).symlink_to(sdk / "lib/armv6/mbedtls/lib" / crypto)
            for name in manifest["archives"]:
                source = "lib" if label == "repaired-without-backup" else "base"
                shutil.copy2(work / source / name, library / name)
            if label == "compiler":
                replace(copied_sdk / "bin/clang", b"unknown compiler")
            elif label == "header":
                replace(
                    copied_sdk / next(iter(manifest["sdk_headers"])), b"unknown header"
                )
            elif label == "crypto":
                replace(library / crypto, b"unknown crypto")
            elif label in {"x509", "tls"}:
                replace(library / ("libmbed" + label + ".a"), b"unknown archive")
            elif label == "backup":
                replace(copied_work / "base/libmbedx509.a", b"unknown backup")
            elif label == "source":
                replace(
                    copied_sdk / "source/mbedtls-symbian/CMakeLists.txt",
                    b"unknown source",
                )
            before = {path.name: digest(path) for path in library.iterdir()}
            completed = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--sdk",
                    str(copied_sdk),
                    "--work-dir",
                    str(copied_work),
                    "--install",
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
            )
            after = {path.name: digest(path) for path in library.iterdir()}
            success = (
                completed.returncode != 0
                and expected in completed.stderr
                and before == after
                and not (copied_work / "objects").exists()
                and not (copied_work / "host").exists()
                and not (copied_work / "installation.json").exists()
            )
            results.append(
                {
                    "case": label,
                    "status": "PASS" if success else "FAIL",
                    "returncode": completed.returncode,
                    "stderr": completed.stderr.strip(),
                    "sdk_archives_unchanged": before == after,
                }
            )
    result = {
        "status": "PASS"
        if all(item["status"] == "PASS" for item in results)
        else "FAIL",
        "checks": len(results),
        "manifest_sha256": digest(MANIFEST),
        "results": results,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"{result['status']}: {len(results)} real SDK input-rejection checks")
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
