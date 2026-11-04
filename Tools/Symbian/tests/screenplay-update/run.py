#!/usr/bin/env python3
"""Compile and check the production browser-private Surface Update client on a host."""
# Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

TEST_DIR = Path(__file__).resolve().parent
BROWSER_DIR = TEST_DIR.parent.parent / "browser"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check(args, build_dir):
    browser_dir = args.browser_dir.resolve()
    sources = [browser_dir / name for name in (
        "ScreenPlayUpdateSession.cpp", "ScreenPlayUpdateSession.h", "ScreenPlayUpdateProtocol.h")]
    for source in sources:
        if not source.is_file():
            raise ValueError(f"Production source is missing: {source}")
    compiler = shlex.split(args.cxx)
    if not compiler:
        raise ValueError("--cxx must name a host C++ compiler")
    executable = build_dir / ("lifecycle.exe" if os.name == "nt" else "lifecycle")
    command = [*compiler, "-std=c++17", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
    if args.sanitize:
        command += ["-fsanitize=address,undefined", "-fsanitize-address-use-after-scope",
                    "-fno-omit-frame-pointer", "-fno-sanitize-recover=all"]
    command += ["-I", str(TEST_DIR / "fake-sdk"), "-I", str(browser_dir),
                str(sources[0]), str(TEST_DIR / "lifecycle.cpp"), "-o", str(executable)]
    subprocess.run(command, check=True, timeout=60)
    completed = subprocess.run([str(executable)], check=True, capture_output=True,
                               text=True, timeout=15)
    if completed.stderr:
        print(completed.stderr, file=sys.stderr, end="")
    result = json.loads(completed.stdout)
    if result.get("result") != "pass" or result.get("guest_executed") is not False:
        raise ValueError("Unexpected host lifecycle result")
    result.update({
        "source_files": {source.name: sha256(source) for source in sources},
        "harness_sha256": sha256(TEST_DIR / "lifecycle.cpp"),
        "sanitizers": ["address", "undefined", "address-use-after-scope"] if args.sanitize else [],
        "compile_command": command,
        "limits": "Host fakes check client IPC construction and lifecycle; no guest, server, device, display or performance validation.",
    })
    return result


def write_result(result, output):
    rendered = json.dumps(result, indent=2) + "\n"
    if output:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(rendered)
    print(rendered, end="")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser-dir", type=Path, default=BROWSER_DIR,
                        help="Production browser source directory (default: sibling browser directory)")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"),
                        help="GCC or Clang C++17 compiler command (default: CXX or c++)")
    parser.add_argument("--sanitize", action="store_true",
                        help="Enable AddressSanitizer, UBSan and address use-after-scope checks")
    parser.add_argument("--build-dir", type=Path,
                        help="Keep the compiled host test in this directory (default: temporary directory)")
    parser.add_argument("--output", type=Path, help="Also write the JSON result to this file")
    args = parser.parse_args()
    try:
        if args.build_dir:
            build_dir = args.build_dir.resolve()
            build_dir.mkdir(parents=True, exist_ok=True)
            result = check(args, build_dir)
        else:
            with tempfile.TemporaryDirectory(prefix="screenplay-update-") as directory:
                result = check(args, Path(directory))
        write_result(result, args.output)
        return 0
    except subprocess.CalledProcessError as error:
        if error.stdout:
            print(error.stdout, file=sys.stderr, end="")
        if error.stderr:
            print(error.stderr, file=sys.stderr, end="")
        failure = f"Host lifecycle check failed (exit {error.returncode})"
        print(failure, file=sys.stderr)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        failure = f"Host lifecycle check failed: {error}"
        print(failure, file=sys.stderr)
    write_result({"result": "fail", "error": failure, "guest_executed": False}, args.output)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
