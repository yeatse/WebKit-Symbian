#!/usr/bin/env python3
"""Compile and exercise the ScreenPlay presenter with host API adapters."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=Path(__file__).resolve().parents[2] / "browser")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    source, here = args.source_dir.resolve(), Path(__file__).resolve().parent
    inputs = [source / "ScreenPlaySurface.h", source / "ScreenPlaySurface.cpp"]
    if not all(path.is_file() for path in inputs):
        parser.error("provide the production ScreenPlaySurface.h/.cpp directory")
    compiler = shutil.which("g++")
    if not compiler:
        parser.error("g++ is required")
    with tempfile.TemporaryDirectory(prefix="screenplay-host-") as temporary:
        temporary = Path(temporary)
        snapshot = temporary / "source"
        snapshot.mkdir()
        # The staged presenter resolves SDK includes through the host adapter.
        copied = []
        for path in inputs:
            target = snapshot / path.name
            target.write_bytes(path.read_bytes())
            copied.append(target)
        binary = temporary / "functional-tests"
        command = [compiler, "-std=c++20", "-O1", "-g", "-Wall", "-Wextra",
                   "-Wno-misleading-indentation", "-Wno-mismatched-new-delete",
                   "-fsanitize=undefined", "-fno-sanitize-recover=all",
                   "-I", str(here / "fake-sdk"), "-I", str(snapshot),
                   str(copied[1]), str(here / "fake-sdk/adapter.cpp"),
                   str(here / "tests.cpp"), "-o", str(binary)]
        build = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if build.returncode:
            raise RuntimeError("presenter compile failed: " + build.stderr[:8000])
        run = subprocess.run([str(binary)], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
        if run.returncode or run.stderr:
            raise RuntimeError("presenter test failed: " + run.stdout + run.stderr)
        match = re.search(r"^RESULT cases=(\d+) checks=(\d+)$", run.stdout, re.M)
        if not match:
            raise RuntimeError("missing presenter result")
        report = {"status": "PASS", "cases": int(match[1]),
                  "assertions": int(match[2]), "sanitizer": "undefined"}
        if args.output:
            args.output.mkdir(parents=True, exist_ok=True)
            (args.output / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
            (args.output / "test.log").write_text(run.stdout)
        print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
