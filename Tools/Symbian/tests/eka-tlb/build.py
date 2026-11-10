#!/usr/bin/env python3
"""Build isolated real-Dynarmic controls from an existing EKA build; never run Ninja builds."""

import argparse
import hashlib
import json
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

BASE_REVISION = "855e885288fc21a616ed07ee4437f34abe7c3922"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--eka-source", type=Path, required=True)
    parser.add_argument("--eka-build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = args.eka_source.resolve()
    build = args.eka_build.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    dynarmic = source / "src/external/dynarmic"
    archive = build / "src/external/dynarmic/src/dynarmic/libdynarmic.a"
    original_hash = digest(archive)
    base_headers = output / "base-headers"
    for header in ("interface/tlb.h", "backend/x64/emit_x64_memory.h"):
        path = base_headers / "dynarmic" / header
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(
            subprocess.check_output(
                [
                    "git",
                    "-C",
                    str(dynarmic),
                    "show",
                    f"{BASE_REVISION}:src/dynarmic/{header}",
                ]
            )
        )

    commands = []

    def run(command, label):
        commands.append(command)
        with (output / f"{label}.log").open("w") as log:
            subprocess.run(
                command, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True
            )

    compiler = None
    for phase in ("old", "corrected"):
        phase_dir = output / phase
        phase_dir.mkdir()
        objects = []
        for unit in ("a32_emit_x64_memory", "a64_emit_x64_memory"):
            target = f"src/external/dynarmic/src/dynarmic/CMakeFiles/dynarmic.dir/backend/x64/{unit}.cpp.o"
            command = subprocess.check_output(
                ["ninja", "-C", str(build), "-t", "commands", target], text=True
            ).splitlines()[-1]
            tokens = shlex.split(command)
            if Path(tokens[0]).name == "ccache":
                tokens = tokens[1:]
            compiler = tokens[0]
            filtered = []
            index = 0
            while index < len(tokens):
                if tokens[index] in ("-include", "-MT", "-MF", "-o"):
                    index += 2  # Do not reuse a PCH built with other headers.
                elif tokens[index] == "-MD":
                    index += 1
                else:
                    filtered.append(tokens[index])
                    index += 1
            if phase == "old":
                filtered.insert(1, f"-I{base_headers}")
            obj = phase_dir / f"{unit}.cpp.o"
            run([*filtered, "-o", str(obj)], f"{phase}-{unit}")
            objects.append(str(obj))
        private_archive = phase_dir / "libdynarmic.a"
        shutil.copyfile(archive, private_archive)
        run(["ar", "r", str(private_archive), *objects], f"{phase}-archive")
        includes = [f"-I{dynarmic / 'src'}", f"-I{dynarmic / 'externals/mcl/include'}"]
        if phase == "old":
            includes.insert(0, f"-I{base_headers}")
        libraries = [
            "src/external/dynarmic/externals/mcl/src/libmcl.a",
            "src/external/fmt/libfmt.a",
            "src/external/dynarmic/externals/zydis/libZydis.a",
            "src/external/dynarmic/externals/zydis/zycore/libZycore.a",
        ]
        run(
            [
                compiler,
                "-std=c++20",
                "-O2",
                *includes,
                str(Path(__file__).with_name("probe.cpp")),
                str(private_archive),
                *[str(build / path) for path in libraries],
                "-pthread",
                "-o",
                str(phase_dir / "probe"),
            ],
            f"{phase}-probe",
        )

    run(
        [
            sys.executable,
            str(Path(__file__).with_name("run.py")),
            "--old",
            str(output / "old/probe"),
            "--new",
            str(output / "corrected/probe"),
            "--output",
            str(output / "results"),
        ],
        "regression",
    )
    if digest(archive) != original_hash:
        raise RuntimeError("shared Dynarmic archive changed during the isolated test")
    manifest = {
        "base_revision": BASE_REVISION,
        "shared_archive_sha256": original_hash,
        "commands": commands,
        "sources": {
            path: digest(dynarmic / "src/dynarmic" / path)
            for path in (
                "interface/tlb.h",
                "backend/x64/emit_x64_memory.h",
                "backend/arm64/emit_arm64_memory.cpp",
            )
        },
        "artifacts": {
            str(path.relative_to(output)): digest(path)
            for path in output.rglob("*")
            if path.is_file()
        },
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print((output / "regression.log").read_text(), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
