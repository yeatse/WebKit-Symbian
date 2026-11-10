#!/usr/bin/env python3
"""Rebuild and verify SDK 0.3.0's two IP-identity objects before installation."""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCHES = ROOT / "patches/sdk-0.3.0/mbedtls-ip"
TESTS = ROOT / "tests/tls-ip-san"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(arguments, **kwargs):
    return subprocess.run(list(map(str, arguments)), check=True, timeout=240, **kwargs)


def read(arguments):
    return run(arguments, capture_output=True, text=True).stdout


def symbols(path):
    result = {}
    for line in read(["readelf", "--wide", "-Ws", path]).splitlines():
        fields = line.split()
        if len(fields) < 8 or not fields[0].rstrip(":").isdigit():
            continue
        _, _, size, kind, binding, visibility, index, name = fields[:8]
        if binding not in {"GLOBAL", "WEAK"}:
            continue
        result[name] = {
            "type": kind,
            "binding": binding,
            "visibility": visibility,
            "defined": index != "UND",
            "size": int(size) if kind != "FUNC" else None,
        }
    return result


def verify_object(old, new, allowed):
    previous, current = symbols(old), symbols(new)
    definitions = lambda items: {
        name: value for name, value in items.items() if value["defined"]
    }
    if definitions(previous) != definitions(current):
        raise SystemExit("Public symbol metadata changed: " + new.name)
    imports = lambda items: {
        name: value for name, value in items.items() if not value["defined"]
    }
    old_imports, new_imports = imports(previous), imports(current)
    if any(new_imports.get(name) != value for name, value in old_imports.items()):
        raise SystemExit("Existing import changed: " + new.name)
    added = set(new_imports) - set(old_imports)
    if added != set(allowed):
        raise SystemExit("Unexpected new imports: " + repr(added))
    if read(["readelf", "-A", old]) != read(["readelf", "-A", new]):
        raise SystemExit("ARM attributes changed: " + new.name)
    fields = ["Class:", "Data:", "Type:", "Machine:", "Flags:"]

    def elf_header(path):
        return [
            line.strip()
            for line in read(["readelf", "-h", path]).splitlines()
            if any(line.strip().startswith(field) for field in fields)
        ]

    if elf_header(old) != elf_header(new):
        raise SystemExit("ELF architecture/flags changed: " + new.name)
    return {
        "public_definitions": len(definitions(current)),
        "added_imports": sorted(added),
    }


def archive_members(ar, archive):
    names = read([ar, "t", archive]).splitlines()
    if len(names) != len(set(names)):
        raise SystemExit("Duplicate archive member")
    return names, {
        name: run([ar, "p", archive, name], capture_output=True).stdout
        for name in names
    }


def host_regression(source, sdk, work, jobs):
    wrapper = work / "host-wrapper"
    wrapper.mkdir(exist_ok=True)
    (wrapper / "CMakeLists.txt").write_text("""cmake_minimum_required(VERSION 3.28)
project(MbedTLSIPHost LANGUAGES C CXX)
add_library(absl::nullability INTERFACE IMPORTED)
set_target_properties(absl::nullability PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${TLS_ABSEIL}")
set(SYMBIAN_MBEDTLS_HOST ON CACHE BOOL "")
set(SYMBIAN_MBEDTLS_TLS13 ON CACHE BOOL "")
set(SYMBIAN_MBEDTLS_OPENSSL_COMPAT OFF CACHE BOOL "")
set(SYMBIAN_MBEDTLS_TESTS OFF CACHE BOOL "")
set(SYMBIAN_MBEDTLS_GUEST_PROBE OFF CACHE BOOL "")
add_subdirectory("${TLS_SOURCE}" tls)
""")
    host = work / "host"
    with (work / "host-build.log").open("w") as log:
        run(
            [
                "cmake",
                "-S",
                wrapper,
                "-B",
                host,
                "-G",
                "Ninja",
                "-DTLS_SOURCE=" + str(source),
                "-DTLS_ABSEIL=" + str(sdk / "include/abseil"),
                "-DSYMBIAN_SDK_PREFIX=" + str(sdk),
                "-DCMAKE_BUILD_TYPE=Release",
            ],
            stdout=log,
            stderr=subprocess.STDOUT,
        )
        run(
            [
                "cmake",
                "--build",
                host,
                "--target",
                "mbedtls",
                "mbedx509",
                "mbedcrypto",
                "-j",
                jobs,
            ],
            stdout=log,
            stderr=subprocess.STDOUT,
        )
    # Each run uses new signed fixtures and new output directories. Never reuse
    # cached PASS markers or depend on the lifetime of old certificates.
    with tempfile.TemporaryDirectory(prefix="regression-", dir=work) as temporary:
        stage = Path(temporary)
        with (work / "regression.log").open("w") as log:
            run(
                [sys.executable, TESTS / "make-certificates.py", stage / "fixtures"],
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            environment = dict(os.environ, CC=os.environ.get("HOST_CC", "cc"))
            run(
                [
                    "bash",
                    TESTS / "build-host.sh",
                    source,
                    host / "tls/library",
                    sdk / "include/abseil",
                    stage / "bin",
                ],
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            run(
                [
                    sys.executable,
                    TESTS / "run.py",
                    "--probe",
                    stage / "bin/probe",
                    "--fixtures",
                    stage / "fixtures",
                    "--output",
                    stage / "certificates",
                ],
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            run(
                [
                    sys.executable,
                    TESTS / "run-parser.py",
                    "--probe",
                    stage / "bin/parser",
                    "--output",
                    stage / "parser",
                ],
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        certificate = json.loads((stage / "certificates/result.json").read_text())
        parser = json.loads((stage / "parser/result.json").read_text())
        if certificate["status"] != "PASS" or certificate["total"] != 35:
            raise SystemExit("Incomplete certificate/ClientHello regression")
        if parser["status"] != "PASS" or parser["cases"] != 2069:
            raise SystemExit("Incomplete portable IP parser regression")
        shutil.copy2(
            stage / "certificates/result.json", work / "certificate-regression.json"
        )
        shutil.copy2(stage / "parser/result.json", work / "parser-regression.json")
    return {"certificates_and_client_hello": certificate, "portable_parser": parser}


def main():
    default_work = Path(
        os.environ.get("SYMBIAN_WEBKIT_WORK", ROOT.parents[1] / "WebKitBuild/Symbian")
    ).resolve()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sdk",
        type=Path,
        default=Path(
            os.environ.get("SYMBIAN_SDK_PREFIX", default_work / "symbian-sdk")
        ),
    )
    parser.add_argument(
        "--work-dir", type=Path, default=default_work / "sdk-mbedtls-ip"
    )
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--install", action="store_true")
    args = parser.parse_args()
    sdk, work = args.sdk.resolve(), args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((PATCHES / "manifest.json").read_text())
    installed = sdk / "lib/armv6/mbedtls/lib"
    if digest(sdk / "bin/clang") != manifest["compiler_sha256"]:
        raise SystemExit("Unknown SDK compiler; expected pinned Linux SDK 0.3.0")
    if digest(installed / "libmbedcrypto.a") != manifest["crypto_sha256"]:
        raise SystemExit("Unknown crypto archive; refusing mixed SDK inputs")
    for name, expected in manifest["sdk_headers"].items():
        if digest(sdk / name) != expected:
            raise SystemExit("SDK header mismatch: " + name)
    original = {}
    base = work / "base"
    base.mkdir(exist_ok=True)
    for name, record in manifest["archives"].items():
        original[name] = digest(installed / name)
        if original[name] not in {record["stock_sha256"], record["patched_sha256"]}:
            raise SystemExit("Unknown installed archive: " + name)
        backup = base / name
        if not backup.exists():
            if original[name] != record["stock_sha256"]:
                raise SystemExit("Original archive backup required for " + name)
            shutil.copy2(installed / name, backup)
        if digest(backup) != record["stock_sha256"]:
            raise SystemExit("Original archive backup changed: " + name)
    source = work / "source"
    if source.exists():
        shutil.rmtree(source)
    for name, expected in manifest["source_files"].items():
        path = sdk / "source/mbedtls-symbian" / name
        if digest(path) != expected:
            raise SystemExit("SDK source mismatch: " + name)
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    patch = PATCHES / "0001-strict-ip-identities.patch"
    if digest(patch) != manifest["patch_sha256"]:
        raise SystemExit("Patch hash mismatch")
    run(
        ["patch", "-p1", "--batch", "--forward", "--fuzz=0", "-i", patch],
        cwd=source,
        stdout=subprocess.DEVNULL,
    )
    for name, expected in manifest["patched_source_sha256"].items():
        if digest(source / name) != expected:
            raise SystemExit("Patched source mismatch: " + name)
    objects, output = work / "objects", work / "lib"
    objects.mkdir(exist_ok=True)
    output.mkdir(exist_ok=True)
    with (work / "arm-build.log").open("w") as log:
        for template in manifest["compile_argv"].values():
            command = [
                arg.format(sdk=sdk, source=source, objects=objects) for arg in template
            ]
            run(command, cwd=work, stdout=log, stderr=subprocess.STDOUT)
    ar = sdk / "bin/llvm-ar"
    verification = {}
    for name, record in manifest["archives"].items():
        member = record["changed_member"]
        archive = output / name
        shutil.copy2(base / name, archive)
        run([ar, "rD", archive, objects / member])
        old_names, old_members = archive_members(ar, base / name)
        new_names, new_members = archive_members(ar, archive)
        changed = [
            item for item in old_names if old_members[item] != new_members.get(item)
        ]
        if old_names != new_names or changed != [member]:
            raise SystemExit("Unexpected archive member changes: " + name)
        old_object = objects / ("stock-" + member)
        old_object.write_bytes(old_members[member])
        checked = verify_object(
            old_object, objects / member, manifest["allowed_added_imports"][member]
        )
        if digest(archive) != record["patched_sha256"]:
            raise SystemExit("Rebuilt archive does not match pinned output: " + name)
        verification[name] = {
            **record,
            **checked,
            "members": len(old_names),
            "changed": changed,
            "member_sha256": {
                item: hashlib.sha256(data).hexdigest()
                for item, data in new_members.items()
            },
        }
    tests = host_regression(source, sdk, work, args.jobs)
    if digest(installed / "libmbedcrypto.a") != manifest["crypto_sha256"]:
        raise SystemExit("SDK crypto archive changed during verification")
    result = {
        "status": "PASS",
        "sdk": str(sdk),
        "archives": verification,
        "crypto_unchanged_sha256": manifest["crypto_sha256"],
        "manifest_sha256": digest(PATCHES / "manifest.json"),
        "compiler_sha256": manifest["compiler_sha256"],
        "certificate_checks": tests["certificates_and_client_hello"]["total"],
        "parser": tests["portable_parser"],
        "installed": False,
    }
    if args.install:
        for name, expected in original.items():
            if digest(installed / name) != expected:
                raise SystemExit("SDK archive changed during verification: " + name)
        for name in original:
            temporary = installed / (name + ".new-" + str(os.getpid()))
            shutil.copy2(output / name, temporary)
            os.replace(temporary, installed / name)
        result["installed"] = True
        (work / "installation.json").write_text(json.dumps(result, indent=2) + "\n")
    (work / "verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print("Two SDK TLS objects, archive ABI and certificate/parser regressions PASS")
    for name, record in manifest["archives"].items():
        print(record["patched_sha256"], name)


if __name__ == "__main__":
    main()
