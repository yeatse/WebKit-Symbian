#!/usr/bin/env python3
"""Build complete pinned LLVM complex helpers; install only with --install."""

from __future__ import annotations

import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "patches/sdk-0.3.0/complex/manifest.json"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def regular(path):
    require(path.is_file() and not path.is_symlink(), f"Not a regular file: {path}")
    return path.read_bytes()


def output(argv):
    return subprocess.check_output([str(value) for value in argv])


def run(argv, log, **kwargs):
    subprocess.run(
        [str(value) for value in argv],
        check=True,
        stdout=log,
        stderr=subprocess.STDOUT,
        **kwargs,
    )


def members(ar, archive):
    names = output([ar, "t", archive]).decode().splitlines()
    require(len(names) == len(set(names)), "Duplicate archive member names")
    return names


def member(ar, archive, name):
    return output([ar, "p", archive, name])


def arm_object(data):
    require(data[:7] == b"\x7fELF\x01\x01\x01", "Expected ELF32 little endian")
    require(struct.unpack_from("<H", data, 18)[0] == 40, "Expected ARM object")
    flags = struct.unpack_from("<I", data, 36)[0]
    require(flags >> 24 == 5 and not flags & 0x400, "Expected EABI5/base PCS")


def definitions(nm, path):
    result = {}
    for line in output([nm, "-P", "-S", "-g", "--defined-only", path]).decode().splitlines():
        fields = line.split()
        if len(fields) >= 4 and fields[1] in ("T", "t"):
            result[fields[0]] = int(fields[3], 16)
    return result


def acquire(spec, source):
    for name, expected in spec["upstream_files"].items():
        path = source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists():
            url = (
                "https://raw.githubusercontent.com/"
                + spec["upstream_repository"]
                + "/"
                + spec["upstream_revision"]
                + "/"
                + name
            )
            with urllib.request.urlopen(url, timeout=60) as response:
                data = response.read(1024 * 1024 + 1)
            require(len(data) <= 1024 * 1024, "Unexpected source size")
            require(digest(data) == expected, f"Download SHA256 mismatch: {name}")
            path.write_bytes(data)
        require(digest(regular(path)) == expected, f"Source SHA256 mismatch: {name}")


def project(source):
    (source / "canary.cpp").write_text(
        "#include <complex>\n"
        'extern "C" void _start(double a,double b,double c,double d,double* out) {\n'
        "std::complex<double> l(a,b),r(c,d); auto p=l*r; auto q=l/r;\n"
        "out[0]=p.real();out[1]=p.imag();out[2]=q.real();out[3]=q.imag();}\n"
    )
    (source / "CMakeLists.txt").write_text(
        """cmake_minimum_required(VERSION 3.28)
project(SDKComplexRuntime LANGUAGES C CXX)
include(SymbianApp)
set(builtins "${CMAKE_SOURCE_DIR}/compiler-rt/lib/builtins")
foreach(profile IN ITEMS default streams native_atomic64)
  add_library(sdk_complex_${profile} OBJECT "${builtins}/muldc3.c" "${builtins}/divdc3.c")
  add_library(sdk_complex_canary_${profile} OBJECT canary.cpp)
  if(profile STREQUAL "streams")
    set(runtime Symbian::Streams)
  elseif(profile STREQUAL "native_atomic64")
    set(runtime Symbian::NativeAtomics64)
  else()
    set(runtime Symbian::Runtime)
  endif()
  foreach(target IN ITEMS sdk_complex_${profile} sdk_complex_canary_${profile})
    target_link_libraries(${target} PRIVATE ${runtime})
    target_compile_options(${target} PRIVATE -fno-fast-math -ffp-contract=off)
  endforeach()
endforeach()
"""
    )


def build(args):
    spec = json.loads(regular(MANIFEST))
    sdk, owner, work = args.sdk.resolve(), args.source.resolve(), args.output.resolve()
    require(spec["format"] == 1 and spec["target_arch"] == "armv6", "Manifest scope")
    require((sdk / "VERSION").read_text().strip() == spec["sdk_version"], "SDK version")
    require(not work.is_relative_to(sdk), "Work directory must be outside SDK")
    require(
        digest(regular(owner / spec["SDK0009_owner_path"])) == spec["SDK0009_owner_sha256"],
        "SDK0009 owner CMake postimage required",
    )
    require(
        digest(regular(sdk / "include/config/math.h")) == spec["math_header_sha256"],
        "SDK0007 math must be installed first",
    )
    require(
        spec["compiler_version"] in output([sdk / "bin/clang", "--version"]).decode(),
        "Pinned compiler version required",
    )
    work.mkdir(parents=True, exist_ok=True)
    source, tree, staged = work / "source", work / "build", work / "stage"
    source.mkdir(exist_ok=True)
    staged.mkdir(exist_ok=True)
    acquire(spec, source)
    project(source)
    with (work / "build.log").open("w") as log:
        run(
            [
                "cmake", "-S", source, "-B", tree, "-G", "Ninja",
                f"-DCMAKE_TOOLCHAIN_FILE={sdk / 'cmake/symbian-arm.cmake'}",
                f"-DSYMBIAN_SDK_PREFIX={sdk}", "-DSYMBIAN_TARGET_ARCH=armv6",
                "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            ],
            log,
        )
        # Cached work is source reuse, never authority for prebuilt objects/stage.
        run(["cmake", "--build", tree, "--clean-first", "-j", "1"], log)
    ar, nm = sdk / "bin/llvm-ar", sdk / "bin/llvm-nm"
    plans = []
    for profile, relative in spec["profiles"].items():
        archive = sdk / relative
        before = regular(archive)
        original_names = members(ar, archive)
        selected = [name for name in original_names if name in spec["members"]]
        require(not selected or len(selected) == 2, "Partial complex member pair refused")
        hashes = {
            name: digest(member(ar, archive, name))
            for name in original_names if name not in spec["members"]
        }
        require(hashes.get("math_c.cc.obj") == spec["math_member_sha256"], "SDK0007 math member mismatch")
        objects = {}
        for name, symbol in spec["members"].items():
            path = tree / f"CMakeFiles/sdk_complex_{profile}.dir/compiler-rt/lib/builtins" / name
            data = regular(path)
            arm_object(data)
            defs = definitions(nm, path)
            require(set(defs) == {symbol} and defs[symbol] > 0, "Real complete builtin definition required")
            if name in selected:
                require(member(ar, archive, name) == data, "Unknown existing complex member refused")
            objects[name] = {"path": str(path), "sha256": digest(data), "bytes": len(data), "function_bytes": defs[symbol]}
        # Always overwrite a cached stage from the current actual prefix.
        stage = staged / archive.name
        stage.write_bytes(before)
        with (work / f"archive-{profile}.log").open("w") as log:
            run([ar, "rcsD", stage, *(item["path"] for item in objects.values())], log)
        expected_names = original_names if selected else original_names + list(spec["members"])
        require(members(ar, stage) == expected_names, "Only the complete pair may be appended/replaced")
        for name, expected in hashes.items():
            require(digest(member(ar, stage, name)) == expected, f"Original member changed: {name}")
        for name, item in objects.items():
            require(digest(member(ar, stage, name)) == item["sha256"], "New member mismatch")
        canary = tree / f"CMakeFiles/sdk_complex_canary_{profile}.dir/canary.cpp.obj"
        image = work / f"canary-{profile}.elf"
        link = [
            sdk / "bin/ld.lld", "-m", "armelf", "--target1-abs", "--no-undefined",
            "--emit-relocs", "--build-id=none", "-e", "_start", "--gc-sections",
            canary, stage, "--as-needed", sdk / "proxies/libm/libm.dso",
            sdk / "proxies/libc/libc.dso", sdk / "proxies/euser/euser.dso",
            sdk / "proxies/libpthread/libpthread.dso", "-o", image,
        ]
        with (work / f"canary-{profile}.log").open("w") as log:
            run(link, log)
        # Hidden SDK definitions may be local in a final linked ELF.
        symbols = output([nm, "-P", "-S", "--defined-only", image]).decode().splitlines()
        for symbol in spec["members"].values():
            rows = [line.split() for line in symbols if line.split() and line.split()[0] == symbol]
            require(len(rows) == 1 and rows[0][1] in ("T", "t") and int(rows[0][3], 16) > 0, "Canary must select exactly one nonempty builtin")
        plans.append({
            "profile": profile, "archive": str(archive), "stage": str(stage),
            "before_sha256": digest(before), "after_sha256": digest(regular(stage)),
            "unchanged_original_members": hashes, "objects": objects,
            "existing_exact_pair": bool(selected), "canary_elf_sha256": digest(regular(image)),
            "canary_elf_bytes": image.stat().st_size, "canary_link_argv": [str(value) for value in link],
        })
    if args.install:
        backups = work / "install-backups"
        backups.mkdir(exist_ok=True)
        with (sdk / "lib/armv6/.complex-builtins.install.lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            for item in plans:
                require(digest(regular(Path(item["archive"]))) == item["before_sha256"], "Prefix changed before install")
                backup = backups / (Path(item["archive"]).name + "." + item["before_sha256"])
                if backup.exists():
                    require(digest(regular(backup)) == item["before_sha256"], "Backup mismatch")
                else:
                    backup.write_bytes(regular(Path(item["archive"])))
                item["backup"] = str(backup)
            replaced = []
            try:
                for item in plans:
                    target = Path(item["archive"])
                    temporary = target.with_name(target.name + ".complex-builtins.tmp")
                    require(not temporary.exists(), "Install temporary file already exists")
                    temporary.write_bytes(regular(Path(item["stage"])))
                    shutil.copymode(target, temporary)
                    os.replace(temporary, target)
                    replaced.append(item)
                for item in plans:
                    require(digest(regular(Path(item["archive"]))) == item["after_sha256"], "Installed archive mismatch")
            except BaseException:
                for item in replaced:
                    shutil.copyfile(item["backup"], item["archive"])
                raise
    report = {
        "status": "INSTALLED" if args.install else "STAGED",
        "installed": args.install, "upstream_revision": spec["upstream_revision"],
        "source_files": spec["upstream_files"], "profiles": plans,
        "original_members_unchanged": sum(len(item["unchanged_original_members"]) for item in plans),
        "scope": "Complete unmodified upstream builtins and actual std::complex ARM ELF links; no guest/loader execution. Only three archive files can be installed, with caught-exception rollback; not a cross-file power-failure transaction. Headers/source remain unchanged.",
    }
    (work / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: value for key, value in report.items() if key != "profiles"}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--install", action="store_true")
    build(parser.parse_args())


if __name__ == "__main__":
    main()
