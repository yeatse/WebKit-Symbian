#!/usr/bin/env python3
"""Restore pinned ScreenPlay headers and frozen import proxies to SDK 0.3.0.

This supplies interfaces only. It neither installs firmware implementations
nor changes executable capabilities or the SDK runtime archives.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OVERLAY = ROOT / "patches/sdk-0.3.0/screenplay"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def target_path(sdk, relative):
    path = Path(relative)
    if path.is_absolute() or ".." in path.parts:
        raise ValueError("ScreenPlay destination escapes SDK prefix")
    result = sdk / path
    for ancestor in [result, *result.parents]:
        if ancestor == sdk.parent:
            break
        if ancestor.is_symlink():
            raise ValueError("ScreenPlay destination contains a symlink")
    return result


def exact_or_missing(path, data):
    if path.exists() and (not path.is_file() or path.read_bytes() != data):
        raise ValueError("Existing ScreenPlay payload differs: " + str(path))


def run(command):
    subprocess.run([str(value) for value in command], check=True)


def prepare(sdk, converter, work):
    manifest = json.loads((OVERLAY / "manifest.json").read_text())
    if (sdk / "VERSION").read_text().strip() != manifest["sdk_version"]:
        raise ValueError("ScreenPlay overlay requires SDK 0.3.0")
    sources = json.loads(
        (sdk / "share/symbian/native/sources.json").read_text()
    )["sources"]
    revisions = {value["repository"]: value["revision"]
                 for value in sources.values()}
    pending = []
    for record in manifest["headers"] + manifest["libraries"]:
        if revisions.get(record["repository"]) != record["revision"]:
            raise ValueError("SDK ScreenPlay source revision differs")
        data = (OVERLAY / record["destination"]).read_bytes()
        blob = hashlib.sha1(
            b"blob " + str(len(data)).encode() + b"\0" + data
        ).hexdigest()
        if sha(data) != record["sha256"] or blob != record["git_blob_sha1"]:
            raise ValueError("ScreenPlay original source digest differs")
        destination = target_path(sdk, record["destination"])
        exact_or_missing(destination, data)
        pending.append((destination, data))
    module = (OVERLAY / "SymbianScreenPlay.cmake").read_bytes()
    if sha(module) != manifest["cmake_sha256"]:
        raise ValueError("ScreenPlay CMake module digest differs")
    module_path = target_path(sdk, "cmake/SymbianScreenPlay.cmake")
    exact_or_missing(module_path, module)
    pending.append((module_path, module))
    metadata = (OVERLAY / "manifest.json").read_bytes()
    destination = target_path(sdk, "share/symbian/screenplay/manifest.json")
    exact_or_missing(destination, metadata)
    pending.append((destination, metadata))

    app = target_path(sdk, "cmake/SymbianApp.cmake")
    app_before = app.read_bytes()
    anchor = b"include(SymbianNativeSurface)\n"
    addition = b"include(SymbianScreenPlay)\n"
    if app_before.count(anchor) != 1 or app_before.count(addition) > 1:
        raise ValueError("SDK App include anchor is not unique")
    app_after = app_before if addition in app_before else app_before.replace(
        anchor, anchor + addition, 1
    )
    # Validate all headers/definitions before generating a proxy or adding any
    # SDK payload. The work directory is external to the installed prefix.
    work.mkdir(parents=True, exist_ok=True)
    for record in manifest["libraries"]:
        name = record["dll"].removesuffix(".dll")
        directory = work / name
        directory.mkdir(exist_ok=True)
        run([converter, "proxy-sources", "--definition",
             OVERLAY / record["destination"], "--target-dll", record["dll"],
             "--output", directory])
        run([sdk / "bin/clang", "--target=armv5t-none-eabi", "-c",
             directory / "exports.S", "-o", directory / "exports.o"])
        generated = directory / (name + ".dso")
        run([sdk / "bin/ld.lld", "-m", "armelf", "-shared",
             "--hash-style=sysv", "--build-id=none", "--soname=" + name + ".dso",
             "--version-script=" + str(directory / "exports.map"),
             "-T", directory / "proxy.ld", directory / "exports.o",
             "-o", generated])
        data = generated.read_bytes()
        destination = target_path(sdk, "proxies/" + name + "/" + name + ".dso")
        exact_or_missing(destination, data)
        pending.append((destination, data))
    return pending, app, app_before, app_after


def install(sdk, converter, work, check_only=False):
    pending, app, before, after = prepare(sdk, converter, work)
    missing = [path for path, _ in pending if not path.exists()]
    if check_only:
        if missing or before != after:
            raise ValueError("ScreenPlay SDK interface installation is incomplete")
        return {"status": "PASS_INSTALLED", "files_added": 0}
    if app.read_bytes() != before:
        raise ValueError("SDK App include changed during preparation")
    for path, data in pending:
        if path.exists():
            exact_or_missing(path, data)
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("xb") as stream:
            stream.write(data)
        path.chmod(0o644)
    if before != after:
        if app.read_bytes() != before:
            raise ValueError("SDK App include changed during installation")
        with tempfile.NamedTemporaryFile(dir=app.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(after)
        temporary.chmod(stat.S_IMODE(app.stat().st_mode))
        temporary.replace(app)
    return {"status": "PASS_INSTALLED", "files_added": len(missing),
            "app_include_added": before != after,
            "runtime_archives_changed": False, "native_target_executed": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--converter", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    sdk, work = args.sdk.absolute(), args.work.absolute()
    if work == sdk or sdk in work.parents:
        parser.error("--work must be outside the SDK prefix")
    print(json.dumps(install(sdk, args.converter, work, args.check_only)))


if __name__ == "__main__":
    main()
