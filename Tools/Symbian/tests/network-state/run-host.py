#!/usr/bin/env python3
"""Test production native observer and, optionally, actual EKA connection registry.

The optional EKA compile reads Ninja's existing commands; it never builds targets.
"""

import argparse
import hashlib
import json
import shlex
import subprocess
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--eka-root", type=Path)
    parser.add_argument("--eka-build", type=Path)
    args = parser.parse_args()
    if bool(args.eka_root) != bool(args.eka_build):
        parser.error("--eka-root and --eka-build must be supplied together")
    here = Path(__file__).resolve().parent
    repo = here.parents[3]
    native = repo / "Source/WebCore/platform/symbian"
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    results = []

    def run(name, command, cwd=repo, expected=None):
        result = subprocess.run(
            command,
            check=False,
            cwd=cwd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
        (output / (name + ".log")).write_text(result.stdout)
        success = result.returncode == 0 and (
            expected is None or expected in result.stdout
        )
        results.append(
            {
                "name": name,
                "command": command,
                "returncode": result.returncode,
                "pass": success,
                "log": name + ".log",
            }
        )
        (output / "result.json").write_text(
            json.dumps(
                {
                    "status": "RUNNING"
                    if all(item["pass"] for item in results)
                    else "FAIL",
                    "checks": results,
                },
                indent=2,
            )
            + "\n"
        )
        if not success:
            raise SystemExit(f"{name} failed: {output / (name + '.log')}")

    exe = output / "test-native"
    run(
        "native-compile",
        [
            args.cxx,
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Werror",
            # GCC warns solely because inherited new and derived delete are
            # different members; ASan checks their actual calloc/free pairing.
            "-Wno-mismatched-new-delete",
            "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer",
            "-fno-pie",
            "-no-pie",
            "-I" + str(here / "fakes"),
            "-I" + str(native),
            str(here / "test-native.cpp"),
            str(native / "NetworkStateNotifierSymbianNative.cpp"),
            "-o",
            str(exe),
        ],
    )
    run(
        "native-tests",
        [str(exe)],
        expected="PASS: 24 production native observer checks",
    )
    sources = [
        native / "NetworkStateNotifierSymbianNative.cpp",
        native / "NetworkStateNotifierSymbianNative.h",
        here / "test-native.cpp",
        *sorted((here / "fakes").glob("*.h")),
    ]
    if args.eka_root:
        eka = args.eka_root.resolve()
        build = args.eka_build.resolve()
        commands = {}
        for relative in ("socket/connection", "internet/connmonitor"):
            source = eka / "src/emu/services/src" / (relative + ".cpp")
            target = (
                "src/emu/services/CMakeFiles/epocservs.dir/src/" + relative + ".cpp.o"
            )
            lines = subprocess.check_output(
                ["ninja", "-C", str(build), "-t", "commands", target], text=True
            ).splitlines()
            matches = [line for line in lines if str(source) in line and " -c " in line]
            if len(matches) != 1:
                raise SystemExit(
                    f"Expected one compile command for {source}; found {len(matches)}"
                )
            command = shlex.split(matches[0])
            if Path(command[0]).name == "ccache":
                command.pop(0)
            for option in ("-MF", "-MT", "-o"):
                position = command.index(option)
                del command[position : position + 2]
            command += [
                "-O0",
                "-UNDEBUG",
                "-ffunction-sections",
                "-fdata-sections",
                "-o",
                str(output / (source.stem + ".o")),
            ]
            run(source.stem + "-compile", command, cwd=build)
            commands[relative] = command
            sources.append(source)
        command = commands["socket/connection"].copy()
        command[command.index("-c") + 1] = str(here / "test-registry.cpp")
        command[command.index("-o") + 1] = str(output / "test-registry.o")
        run("registry-compile", command, cwd=build)
        run(
            "registry-link",
            [
                args.cxx,
                str(output / "test-registry.o"),
                str(output / "connection.o"),
                "-Wl,--gc-sections",
                "-o",
                str(output / "test-registry"),
            ],
        )
        run(
            "registry-tests",
            [str(output / "test-registry")],
            expected="PASS: 19 real connection registry checks",
        )
        sources.append(here / "test-registry.cpp")
    result = json.loads((output / "result.json").read_text())
    result["status"] = "PASS"
    result["source_sha256"] = {str(path): digest(path) for path in sources}
    result["runtime_scope"] = (
        "host API-boundary simulation and actual EKA registry; no guest or full emulator run"
    )
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        "PASS: native observer" + (" and actual EKA registry" if args.eka_root else "")
    )


if __name__ == "__main__":
    main()
