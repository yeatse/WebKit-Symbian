#!/usr/bin/env python3
"""Privately compile actual EKA sources and test sharing and the physical VFS."""

import argparse
import hashlib
import json
import pathlib
import shlex
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def execute(command, cwd, log):
    with log.open("w") as output:
        subprocess.run(
            command,
            cwd=cwd,
            stdout=output,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=180,
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--eka-source", type=pathlib.Path, required=True)
    parser.add_argument("--eka-build", type=pathlib.Path, required=True)
    parser.add_argument("--overlay", type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    source = args.eka_source.resolve(strict=True)
    build = args.eka_build.resolve(strict=True)
    candidate = args.overlay.resolve(strict=True) if args.overlay else source
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    records = []
    for module, target, relative in (
        ("services", "epocservs", "fs/files.cpp"),
        ("services", "epocservs", "fs/fs.cpp"),
        ("vfs", "epocio", "vfs.cpp"),
    ):
        cpp = candidate / "src/emu" / module / "src" / relative
        ninja_target = f"src/emu/{module}/CMakeFiles/{target}.dir/src/{relative}.o"
        command = shlex.split(
            subprocess.check_output(
                ["ninja", "-C", str(build), "-t", "commands", ninja_target], text=True
            ).splitlines()[-1]
        )
        obj = output / (cpp.stem + ".o")
        for option, value in (
            ("-o", str(obj)),
            ("-MF", str(obj) + ".d"),
            ("-MT", str(obj)),
            ("-c", str(cpp)),
        ):
            command[command.index(option) + 1] = value
        # Insert after the compiler/optional ccache, before configured includes.
        first_option = next(i for i, arg in enumerate(command) if arg.startswith("-"))
        command[first_option:first_option] = [
            "-I" + str(candidate / "src/emu/services/include"),
            "-I" + str(candidate / "src/emu/vfs/include"),
            "-ffunction-sections",
            "-fdata-sections",
        ]
        execute(command, build, output / (cpp.stem + ".log"))
        records.append({"source": str(cpp), "sha256": digest(cpp), "command": command})

    # Full file-server TUs are integration compile checks. Only the actual
    # physical VFS and production sharing/attribute helpers run here, not IPC.
    options = [
        option
        for option in records[-1]["command"]
        if option.startswith("-I") or (option.startswith("-D") and option != "-DNDEBUG")
    ]
    archives = [
        build / path
        for path in (
            "src/emu/common/libcommon.a",
            "src/external/fmt/libfmt.a",
            "src/external/spdlog/libspdlog.a",
            "src/external/re2/libre2.a",
        )
    ]
    compiler = next(
        arg
        for arg in records[-1]["command"]
        if pathlib.Path(arg).name in ("c++", "g++", "clang++")
    )
    probe_source = pathlib.Path(__file__).with_name("probe.cpp").resolve()
    probe = output / "probe"
    link = [
        compiler,
        "-std=c++20",
        "-O2",
        "-ffunction-sections",
        "-fdata-sections",
        *options,
        str(probe_source),
        str(output / "vfs.o"),
        "-Wl,--gc-sections",
        "-Wl,--start-group",
        *map(str, archives),
        "-Wl,--end-group",
        "-lpthread",
        "-o",
        str(probe),
    ]
    execute(link, output, output / "probe-build.log")
    log = subprocess.check_output(
        [str(probe), str(output / "fixture")], text=True, timeout=20
    )
    (output / "probe.log").write_text(log)
    if log != "PASS 115 actual sharing and physical VFS checks\n":
        raise ValueError(f"Unexpected probe output: {log!r}")
    headers = [
        "src/emu/services/include/services/fs/fs.h",
        "src/emu/services/include/services/fs/file_share.h",
        "src/emu/services/include/services/fs/file_attributes.h",
        "src/emu/vfs/include/vfs/vfs.h",
        "src/emu/vfs/include/vfs/host_file_attributes.h",
    ]
    result = {
        "status": "HOST_SHARING_AND_PHYSICAL_VFS_PASS",
        "scope": "Actual sharing helpers and physical VFS; file-server IPC awaits guest validation.",
        "checks": 115,
        "compiled": records,
        "headers": {path: digest(candidate / path) for path in headers},
        "linked_archives": {str(path): digest(path) for path in archives},
        "link_command": link,
        "probe_source_sha256": digest(probe_source),
        "probe_sha256": digest(probe),
    }
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(log, end="")


if __name__ == "__main__":
    main()
