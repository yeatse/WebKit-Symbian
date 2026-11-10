#!/usr/bin/env python3
"""Check EKA2L1 file creation and cached case-insensitive lookups."""

import argparse
import json
import os
import pathlib
import shlex
import shutil
import subprocess

SOURCE = "src/emu/common/src/fileutils.cpp"


def run(command, *, cwd, log):
    subprocess.run(command, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--eka-source", type=pathlib.Path, required=True)
    parser.add_argument("--eka-build", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    source = args.eka_source.resolve(strict=True)
    build = args.eka_build.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    command = subprocess.check_output(
        [
            "ninja", "-C", str(build), "-t", "commands",
            "src/emu/common/CMakeFiles/common.dir/src/fileutils.cpp.o",
        ],
        text=True,
    ).strip().splitlines()[-1]
    compile_command = shlex.split(command)
    staged = output / SOURCE
    staged.parent.mkdir(parents=True)
    shutil.copyfile(source / SOURCE, staged)
    obj = output / "fileutils.cpp.o"
    compile_command[compile_command.index("-o") + 1] = str(obj)
    for option, value in (("-MF", str(output / "fileutils.cpp.o.d")), ("-MT", str(obj))):
        if option in compile_command:
            compile_command[compile_command.index(option) + 1] = value
    source_argument = next(
        i for i, arg in enumerate(compile_command)
        if arg == SOURCE or arg.endswith("/" + SOURCE)
    )
    compile_command[source_argument] = str(staged)
    archive = output / "libcommon.a"
    image = output / "probe"
    link_command = [
        os.environ.get("CXX", "c++"), "-std=c++17", "-O2",
        "-ffunction-sections", "-fdata-sections",
        "-I" + str(source / "src/emu/common/include"),
        "-I" + str(build / "src/emu/common/include"),
        "-I" + str(source / "src/external/fmt/include"),
        "-I" + str(source / "src/external/spdlog/include"),
        str(pathlib.Path(__file__).with_name("probe.cpp").resolve()),
        "-Wl,--gc-sections", "-Wl,--start-group", str(archive),
        str(build / "src/external/fmt/libfmt.a"),
        str(build / "src/external/spdlog/libspdlog.a"),
        str(build / "src/external/re2/libre2.a"),
        "-Wl,--end-group", "-lpthread", "-o", str(image),
    ]
    with (output / "build.log").open("w") as log:
        run(compile_command, cwd=build, log=log)
        shutil.copyfile(build / "src/emu/common/libcommon.a", archive)
        run(["ar", "r", str(archive), str(obj)], cwd=output, log=log)
        run(link_command, cwd=output, log=log)
    observed = json.loads(
        subprocess.check_output([str(image), str(output / "fixture")], text=True)
    )
    expected = {
        key: 1 for key in (
            "write_same_second", "write_alias_found", "append_same_second",
            "append_alias_found", "exact_and_source_preserved",
            "explicit_refresh_finds_both", "collision_directories_preserved",
            "failed_open_safe", "existing_write_works",
        )
    }
    expected["collision_alias_found"] = 0
    if observed != expected:
        raise ValueError(f"Unexpected checks: {observed}; expected {expected}")
    result = {"status": "PASS", "checks": observed}
    report = json.dumps(result, indent=2) + "\n"
    (output / "result.json").write_text(report)
    print(report, end="")


if __name__ == "__main__":
    main()
