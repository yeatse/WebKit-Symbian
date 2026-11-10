#!/usr/bin/env python3
"""Build the real SDK math member; install only with an explicit --install."""

from __future__ import annotations

import argparse
import contextlib
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / "patches/sdk-0.3.0/math/manifest.json"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def regular(path):
    require(
        path.is_file() and not path.is_symlink(), f"Not a regular file: {path}"
    )
    return path.read_bytes()


def run(argv, log, **kwargs):
    subprocess.run(
        [str(x) for x in argv],
        check=True,
        stdout=log,
        stderr=subprocess.STDOUT,
        **kwargs,
    )


def output(argv):
    return subprocess.check_output([str(x) for x in argv])


def symbols(nm, data, directory, name):
    path = directory / name
    path.write_bytes(data)
    lines = (
        output([nm, "-P", "-g", "--defined-only", path]).decode().splitlines()
    )
    return {line.split()[0] for line in lines if len(line.split()) >= 2}


def arm_object(data):
    require(data[:7] == b"\x7fELF\x01\x01\x01", "Expected little-endian ELF32")
    require(struct.unpack_from("<H", data, 18)[0] == 40, "Expected ARM object")
    flags = struct.unpack_from("<I", data, 36)[0]
    require(flags >> 24 == 5 and not flags & 0x400, "Expected EABI5/base PCS")


def project(source, sdk, files):
    for name, data in files.items():
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    header = source / "cpp/symbian/runtime/include/math.h"
    overlay = {
        "version": 0,
        "case-sensitive": "true",
        "roots": [
            {
                "type": "file",
                "name": str(sdk / "include/config/math.h"),
                "external-contents": str(header),
            }
        ],
    }
    (source / "overlay.json").write_text(json.dumps(overlay, indent=2) + "\n")
    (source / "header-canary.cpp").write_text(
        "#include <cmath>\n#include <cfloat>\n#include <type_traits>\n"
        "static_assert(sizeof(long double) == sizeof(double));\n"
        "static_assert(LDBL_MANT_DIG == DBL_MANT_DIG);\n"
        "static_assert(LDBL_MAX_EXP == DBL_MAX_EXP);\n"
        "static_assert(std::is_same_v<decltype(::atan2l(1.L, 1.L)), long double>);\n"
        "long double (*volatile runtime_atan2l)(long double, long double) = ::atan2l;\n"
        "long double use_cxx_math(long double y, long double x) { return std::atan2(y, x); }\n"
    )
    (source / "CMakeLists.txt").write_text(
        """cmake_minimum_required(VERSION 3.28)
project(SDKMathRuntime LANGUAGES CXX)
include(SymbianApp)
foreach(profile IN ITEMS default streams native_atomic64)
  add_library(sdk_math_${profile} OBJECT cpp/symbian/runtime/math_c.cc)
  add_library(sdk_math_header_${profile} OBJECT header-canary.cpp)
  if(profile STREQUAL "streams")
    set(runtime Symbian::Streams)
  elseif(profile STREQUAL "native_atomic64")
    set(runtime Symbian::NativeAtomics64)
  else()
    set(runtime Symbian::Runtime)
  endif()
  foreach(target IN ITEMS sdk_math_${profile} sdk_math_header_${profile})
    target_link_libraries(${target} PRIVATE ${runtime})
    target_compile_options(${target} PRIVATE -ivfsoverlay "${CMAKE_SOURCE_DIR}/overlay.json")
  endforeach()
endforeach()
"""
    )


def build(args):
    spec = json.loads(SPEC.read_text())
    sdk, owner, work = (
        args.sdk.resolve(),
        args.source.resolve(),
        args.output.resolve(),
    )
    require(
        spec["format"] == 1 and spec["target_arch"] == "armv6", "Manifest scope"
    )
    require(
        (sdk / "VERSION").read_text().strip() == spec["sdk_version"],
        "SDK version",
    )
    require(
        work != sdk and not work.is_relative_to(sdk),
        "Output must be outside SDK",
    )
    work.mkdir(parents=True, exist_ok=True)
    files = {}
    for name, expected in spec["sources"].items():
        files[name] = regular(owner / name)
        require(
            digest(files[name]) == expected, f"SDK0007 source mismatch: {name}"
        )
    header_path = sdk / "include/config/math.h"
    before_header = regular(header_path)
    new_header = files["cpp/symbian/runtime/include/math.h"]
    require(
        digest(before_header)
        in (spec["base_header_sha256"], digest(new_header)),
        "Unrecognized installed math header; refusing overwrite",
    )
    before = {
        name: regular(sdk / path) for name, path in spec["profiles"].items()
    }
    compiler = sdk / "bin/clang++"
    ar, nm = sdk / "bin/llvm-ar", sdk / "bin/llvm-nm"
    require(
        "clang version 23." in output([compiler, "--version"]).decode(),
        "Pinned Clang23 required",
    )
    source, tree, staged = work / "source", work / "build", work / "stage"
    source.mkdir(exist_ok=True)
    project(source, sdk, files)
    with (work / "build.log").open("w") as log:
        run(
            [
                "cmake",
                "-S",
                source,
                "-B",
                tree,
                "-G",
                "Ninja",
                f"-DCMAKE_TOOLCHAIN_FILE={sdk / 'cmake/symbian-arm.cmake'}",
                f"-DSYMBIAN_SDK_PREFIX={sdk}",
                "-DSYMBIAN_TARGET_ARCH=armv6",
                "-DCMAKE_BUILD_TYPE=Release",
            ],
            log,
        )
        run(["cmake", "--build", tree, "-j", "1"], log)
    report = {
        "status": "BUILT_NOT_INSTALLED",
        "scope": "ARMv6 math member rebuild only; no loader/guest/audio run",
        "owner_sources": spec["sources"],
        "profiles": {},
        "installed": False,
    }
    targets = []
    for profile, relative in spec["profiles"].items():
        original = sdk / relative
        names = output([ar, "t", original]).decode().splitlines()
        require(
            len(names) == len(set(names)) and names.count(spec["member"]) == 1,
            "Archive members must be unique with exactly one math member",
        )
        old_math = output([ar, "p", original, spec["member"]])
        obj = (
            tree
            / f"CMakeFiles/sdk_math_{profile}.dir/cpp/symbian/runtime/math_c.cc.obj"
        )
        new_math = regular(obj)
        arm_object(new_math)
        require(
            digest(old_math) in (spec["base_member_sha256"], digest(new_math)),
            f"Unrecognized math member: {relative}",
        )
        old_symbols = symbols(nm, old_math, work, f"old-{profile}.obj")
        new_symbols = symbols(nm, new_math, work, f"new-{profile}.obj")
        require(
            new_symbols == old_symbols | {"atan2l"},
            "Unexpected math ABI definitions",
        )
        member_dir = work / "members" / profile
        member_dir.mkdir(parents=True, exist_ok=True)
        member = member_dir / spec["member"]
        member.write_bytes(new_math)
        target = staged / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(before[profile])
        with (work / f"archive-{profile}.log").open("w") as log:
            run([ar, "rD", target, member], log)
        require(
            output([ar, "t", target]).decode().splitlines() == names,
            "Member order changed",
        )
        others = {}
        for name in names:
            first = output([ar, "p", original, name])
            second = output([ar, "p", target, name])
            if name != spec["member"]:
                require(
                    first == second,
                    f"Unrelated member changed: {relative}:{name}",
                )
                others[name] = digest(first)
            else:
                require(second == new_math, "New math member missing")
        report["profiles"][profile] = {
            "path": relative,
            "before_sha256": digest(before[profile]),
            "after_sha256": digest(target.read_bytes()),
            "old_member_sha256": digest(old_math),
            "new_member_sha256": digest(new_math),
            "unchanged_other_members": others,
            "defined_math_symbols": sorted(new_symbols),
        }
        targets.append((original, target.read_bytes(), before[profile]))
    staged_header = staged / "include/config/math.h"
    staged_header.parent.mkdir(parents=True, exist_ok=True)
    staged_header.write_bytes(new_header)
    targets.append((header_path, new_header, before_header))
    require(
        all(regular(path) == old for path, _, old in targets),
        "SDK changed during build",
    )
    report["installed_prefix_unchanged_before_optional_install"] = True
    if args.install:
        with (sdk / ".sdk-math-install.lock").open("a") as lock:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
            require(
                all(regular(path) == old for path, _, old in targets),
                "SDK changed before install",
            )
            backups = work / "install-backups"
            for path, _, old in targets:
                backup = backups / path.relative_to(sdk)
                backup.parent.mkdir(parents=True, exist_ok=True)
                backup.write_bytes(old)
            replaced = []
            try:
                for path, data, old in targets:
                    descriptor, name = tempfile.mkstemp(
                        prefix=".sdk-math-new-", dir=path.parent
                    )
                    try:
                        with os.fdopen(descriptor, "wb") as stream:
                            stream.write(data)
                        os.chmod(name, path.stat().st_mode & 0o777)
                        os.replace(name, path)
                    finally:
                        with contextlib.suppress(FileNotFoundError):
                            os.unlink(name)
                    replaced.append((path, old))
                require(
                    all(regular(path) == data for path, data, _ in targets),
                    "Install verification",
                )
            except BaseException:
                for path, old in reversed(replaced):
                    path.write_bytes(old)
                raise
            report.update(status="INSTALLED", installed=True)
    report["header"] = {
        "before_sha256": digest(before_header),
        "after_sha256": digest(new_header),
    }
    (work / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "status": report["status"],
                "profiles": list(report["profiles"]),
                "installed": report["installed"],
                "output": str(work),
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--install", action="store_true")
    build(parser.parse_args())


if __name__ == "__main__":
    main()
