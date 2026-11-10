#!/usr/bin/env python3
"""Build a bounded native probe with a snapshot of the actual production bridge."""

import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument(
        "--mode", choices=("baseline", "threshold", "browser-worker"), required=True
    )
    args = parser.parse_args()
    sdk = args.sdk.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    project = output / "source"
    project.mkdir()
    tests = pathlib.Path(__file__).resolve().parent
    bridge = tests.parents[3] / "Source/WTF/wtf/symbian"
    inputs = [tests / f"native-{args.mode}.cpp"] + [
        bridge / f"MemoryPressureMonitorSymbianNative.{suffix}"
        for suffix in ("h", "cpp")
    ]
    hashes = {}
    for source in inputs:
        destination = project / ("probe.cpp" if source == inputs[0] else source.name)
        shutil.copyfile(source, destination)
        hashes[source.name] = hashlib.sha256(source.read_bytes()).hexdigest()
    uid = {
        "baseline": "0xe00008f9",
        "threshold": "0xe00008fa",
        "browser-worker": "0xe00008fb",
    }[args.mode]
    (project / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\n"
        "project(memory_pressure_probe LANGUAGES CXX ASM)\n"
        "include(SymbianApp)\n"
        "symbian_add_executable(memory_pressure_probe probe.cpp MemoryPressureMonitorSymbianNative.cpp)\n"
        "symbian_enable_native_leaves(memory_pressure_probe SOURCES probe.cpp)\n"
        "target_compile_options(memory_pressure_probe PRIVATE -marm)\n"
        'target_link_libraries(memory_pressure_probe PRIVATE Symbian::EUser "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso")\n'
        f"symbian_publish_executable(memory_pressure_probe UID3 {uid} CAPABILITIES 0)\n"
    )
    commands = [
        [
            "cmake",
            "-S",
            str(project),
            "-B",
            str(output / "build"),
            "-G",
            "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={sdk}/cmake/symbian-arm.cmake",
            f"-DSYMBIAN_SDK_PREFIX={sdk}",
            "-DCMAKE_BUILD_TYPE=Release",
        ],
        ["cmake", "--build", str(output / "build"), "--parallel", "1"],
    ]
    with (output / "build.log").open("w") as log:
        for command in commands:
            subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
    image = output / "build/e32/memory_pressure_probe.exe"
    result = {
        "mode": args.mode,
        "inputs": hashes,
        "commands": commands,
        "image": str(image),
        "image_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
        "uid3": uid,
        "runtime_executed": False,
    }
    (output / "build.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
