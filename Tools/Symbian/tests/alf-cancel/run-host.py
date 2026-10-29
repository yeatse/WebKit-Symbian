#!/usr/bin/env python3
"""Build actual EKA ALF code against a small IPC/kernel test boundary."""

import argparse
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--eka-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--exit-contract-only", action="store_true")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = parser.parse_args()
    root = args.eka_root.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    here = Path(__file__).resolve().parent
    source = root / "src/emu/services/src/audio/alf/alf.cpp"
    header = root / "src/emu/services/include/services/audio/alf/alf.h"
    command = [
        args.cxx,
        "-std=c++20",
        "-g",
        "-O1",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-fsanitize=address,undefined",
        "-fno-omit-frame-pointer",
        "-fno-pie",
        "-no-pie",
        f"-I{here / 'fakes'}",
        f"-I{root / 'src/emu/services/include'}",
        f"-I{root / 'src/emu/utils/include'}",
        str(here / "test-alf.cpp"),
        str(source),
        "-o",
        str(output / "test-alf"),
    ]
    result = {
        "status": "FAIL",
        "scope": "actual ALF source, fake IPC/kernel boundaries",
    }
    result["inputs"] = {
        str(
            path.relative_to(root)
            if path.is_relative_to(root)
            else path.relative_to(here)
        ): sha256(path)
        for path in [
            source,
            header,
            *sorted(here.rglob("*.h")),
            here / "test-alf.cpp",
            Path(__file__).resolve(),
        ]
    }
    result["command"] = command
    try:
        with (output / "build.log").open("w") as log:
            subprocess.run(
                command, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=90
            )
        environment = dict(
            os.environ,
            ASAN_OPTIONS="detect_leaks=1:halt_on_error=1",
            UBSAN_OPTIONS="halt_on_error=1",
        )
        run = subprocess.run(
            [
                str(output / "test-alf"),
                *(["--exit-contract-only"] if args.exit_contract_only else []),
            ],
            check=True,
            capture_output=True,
            text=True,
            env=environment,
            timeout=15,
        )
        (output / "run.log").write_text(run.stdout + run.stderr)
        match = re.search(r"^RESULT: passed=(\d+) failed=0$", run.stdout, re.MULTILINE)
        if not match:
            raise RuntimeError("missing actual test success marker")
        result.update(
            status="PASS",
            passed=int(match[1]),
            failed=0,
            sanitizers=["address", "undefined"],
        )
    except subprocess.CalledProcessError as error:
        if error.stdout is not None:
            (output / "run.log").write_text((error.stdout or "") + (error.stderr or ""))
        raise
    finally:
        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: result[key] for key in ("status", "passed", "failed")}))


if __name__ == "__main__":
    main()
