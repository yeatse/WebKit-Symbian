#!/usr/bin/env python3
"""Compile the actual presenter against a narrow owned host API adapter."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def identity(path):
    data = path.read_bytes()
    return {"name": path.name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=Path(__file__).resolve().parents[2] / "browser")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    source, here = args.source_dir.resolve(), Path(__file__).resolve().parent
    inputs = [source / "ScreenPlaySurface.h", source / "ScreenPlaySurface.cpp"]
    if not all(path.is_file() for path in inputs):
        parser.error("provide the production ScreenPlaySurface.h/.cpp directory")
    before = [identity(path) for path in inputs]
    compiler = shutil.which("g++")
    if not compiler:
        parser.error("g++ is required for the owned host adapter")
    with tempfile.TemporaryDirectory(prefix="screenplay-owned-host-") as temporary:
        binary = Path(temporary) / "functional-tests"
        command = [compiler, "-std=c++20", "-O1", "-g", "-Wall", "-Wextra",
                   "-Wno-misleading-indentation", "-Wno-mismatched-new-delete",
                   "-fsanitize=undefined", "-fno-sanitize-recover=all",
                   "-I", str(here / "fake-sdk"), "-I", str(source),
                   str(inputs[1]), str(here / "fake-sdk/adapter.cpp"),
                   str(here / "tests.cpp"), "-o", str(binary)]
        build = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if build.returncode:
            raise RuntimeError("owned host compile failed: " + build.stderr[:8000])
        run = subprocess.run([str(binary)], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
        if run.returncode or run.stderr:
            raise RuntimeError("owned host functional test failed; inspect the owned test locally")
        match = re.search(r"^RESULT cases=(\d+) checks=(\d+) scope=actual-source-owned-host-adapter native=false display=false$", run.stdout, re.M)
        if not match:
            raise RuntimeError("missing strict result footer")
        if before != [identity(path) for path in inputs]:
            raise RuntimeError("production inputs changed during the test")
        report = {"status": "PASS_ACTUAL_SOURCE_OWNED_HOST_ADAPTER", "cases": int(match[1]),
                  "assertions": int(match[2]), "sanitizer": "undefined", "source_files": before,
                  "build_exit": build.returncode, "test_exit": run.returncode,
                  "native_ABI_verified": False, "guest_run": False, "display_verified": False,
                  "performance_verified": False, "phone_verified": False,
                  "scope": "Actual production presenter source; owned deterministic API/callback adapter only"}
        if args.output:
            args.output.mkdir(parents=True, exist_ok=True)
            (args.output / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
            (args.output / "test.log").write_text(run.stdout)
        print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
