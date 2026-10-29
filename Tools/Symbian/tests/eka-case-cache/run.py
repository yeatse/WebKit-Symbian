#!/usr/bin/env python3
"""Compile old/new actual EKA fileutils in private archives; never build/run EKA."""

import argparse
import hashlib
import json
import os
import pathlib
import shlex
import shutil
import subprocess

BASE = "5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a"
SOURCE = "src/emu/common/src/fileutils.cpp"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, *, cwd, log):
    subprocess.run(command, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--eka-source", type=pathlib.Path, required=True)
    parser.add_argument("--eka-build", type=pathlib.Path, required=True)
    parser.add_argument("--patch", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    source, build, patch = (
        path.resolve(strict=True)
        for path in (args.eka_source, args.eka_build, args.patch)
    )
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    baseline = subprocess.check_output(["git", "show", BASE + ":" + SOURCE], cwd=source)
    command = (
        subprocess.check_output(
            [
                "ninja",
                "-C",
                str(build),
                "-t",
                "commands",
                "src/emu/common/CMakeFiles/common.dir/src/fileutils.cpp.o",
            ],
            text=True,
        )
        .strip()
        .splitlines()[-1]
    )
    original = shlex.split(command)
    result = {
        "base": BASE,
        "patch_sha256": digest(patch),
        "common_archive_sha256": digest(build / "src/emu/common/libcommon.a"),
        "commands": [],
        "outcomes": {},
    }
    for variant in ("before", "after"):
        work = output / variant
        staged = work / SOURCE
        staged.parent.mkdir(parents=True)
        staged.write_bytes(baseline)
        with (work / "build.log").open("w") as log:
            if variant == "after":
                run(["git", "apply", "--check", str(patch)], cwd=work, log=log)
                run(["git", "apply", str(patch)], cwd=work, log=log)
            obj = work / "fileutils.cpp.o"
            compile_command = list(original)
            compile_command[compile_command.index("-o") + 1] = str(obj)
            # Dependency output is a mutation too: keep both it and its target
            # in the private build, even though the command came from Ninja.
            for option, value in (
                ("-MF", str(work / "fileutils.cpp.o.d")),
                ("-MT", str(obj)),
            ):
                if option in compile_command:
                    compile_command[compile_command.index(option) + 1] = value
            source_argument = next(
                i for i, arg in enumerate(compile_command) if arg.endswith("/" + SOURCE)
            )
            compile_command[source_argument] = str(staged)
            run(compile_command, cwd=build, log=log)
            archive = work / "libcommon.a"
            shutil.copyfile(build / "src/emu/common/libcommon.a", archive)
            run(["ar", "r", str(archive), str(obj)], cwd=work, log=log)
            image = work / "probe"
            link_command = [
                os.environ.get("CXX", "c++"),
                "-std=c++17",
                "-O2",
                "-ffunction-sections",
                "-fdata-sections",
                "-I" + str(source / "src/emu/common/include"),
                "-I" + str(build / "src/emu/common/include"),
                "-I" + str(source / "src/external/fmt/include"),
                "-I" + str(source / "src/external/spdlog/include"),
                str(pathlib.Path(__file__).with_name("probe.cpp").resolve()),
                "-Wl,--gc-sections",
                "-Wl,--start-group",
                str(archive),
                str(build / "src/external/fmt/libfmt.a"),
                str(build / "src/external/spdlog/libspdlog.a"),
                str(build / "src/external/re2/libre2.a"),
                "-Wl,--end-group",
                "-lpthread",
                "-o",
                str(image),
            ]
            run(link_command, cwd=work, log=log)
        raw = subprocess.check_output([str(image), str(work / "fixture")], text=True)
        (work / "result.json").write_text(raw)
        observed = json.loads(raw)
        expected = {
            key: 1
            for key in (
                "write_same_second",
                "append_same_second",
                "exact_and_source_preserved",
                "explicit_refresh_finds_both",
                "collision_directories_preserved",
                "failed_open_safe",
                "existing_write_works",
            )
        }
        expected.update(
            write_alias_found=int(variant == "after"),
            append_alias_found=int(variant == "after"),
            collision_alias_found=0,
        )
        if observed != expected:
            raise ValueError(
                f"{variant} unexpected outcome: {observed}; expected {expected}"
            )
        result["commands"].append(
            {"variant": variant, "compile": compile_command, "link": link_command}
        )
        result["outcomes"][variant] = {
            "source_sha256": digest(staged),
            "object_sha256": digest(obj),
            "probe_sha256": digest(image),
            "observed": observed,
        }
    result["status"] = "PASS_OLD_FAILURE_REPRODUCED_NEW_FIXED"
    result["scope"] = (
        "Actual common file open/resolver on a case-sensitive host; existing duplicate directories retained, not repaired or accepted as a valid Symbian namespace."
    )
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {"status": result["status"], "outcomes": result["outcomes"]}, indent=2
        )
    )


if __name__ == "__main__":
    main()
