#!/usr/bin/env python3
"""Rebuild the pinned SDK 0.3.0 ARMv6 mimalloc objects without raising its budget."""

import argparse
import fcntl
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OVERLAY = ROOT / "patches/sdk-0.3.0"
MANIFEST = OVERLAY / "mimalloc-runtime.json"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def checked_file(path, expected):
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"Expected a regular file: {path}")
    data = path.read_bytes()
    if digest(data) != expected:
        raise ValueError(f"SHA256 mismatch: {path}")
    return data


def run(command, **kwargs):
    return subprocess.run(command, check=True, **kwargs)


def apply_runtime_patch(source, patch, expected_header, log):
    """Apply from an explicit private Git root, even inside another checkout."""
    source = source.resolve()
    metadata = source / ".git"
    if metadata.is_symlink() or (metadata.exists() and not metadata.is_dir()):
        raise ValueError("Patched source Git metadata is not a local directory")
    # A CI checkout is an enclosing Git worktree. Plain `git apply` can silently
    # skip paths relative to that outer root. Inherited GIT_DIR/WORK_TREE/index
    # settings must not redirect this local operation either.
    environment = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith("GIT_")
    }
    environment.update(GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull)
    command = [
        "git",
        "--git-dir=" + str(metadata),
        "--work-tree=" + str(source),
    ]
    options = {"cwd": source, "env": environment, "stdout": log, "stderr": log}
    run(command + ["init", "-q", "--template="], **options)
    run(command + ["apply", "--check", str(patch.resolve())], **options)
    run(command + ["apply", str(patch.resolve())], **options)
    # Exit zero alone is not proof that Git applied a patch.
    checked_file(
        source / "cpp/symbian/runtime/mimalloc_port.h", expected_header
    )


def checkout(path, specification, log):
    if not path.exists():
        path.mkdir(parents=True)
        run(["git", "init", "-q", str(path)], stdout=log, stderr=log)
        run(
            [
                "git",
                "-C",
                str(path),
                "fetch",
                "-q",
                "--depth",
                "1",
                specification["repository"],
                specification["revision"],
            ],
            stdout=log,
            stderr=log,
        )
        run(
            [
                "git",
                "-C",
                str(path),
                "checkout",
                "-q",
                "--detach",
                "FETCH_HEAD",
            ],
            stdout=log,
            stderr=log,
        )
    revision = subprocess.check_output(
        ["git", "-C", str(path), "rev-parse", "HEAD"], text=True
    ).strip()
    if revision != specification["revision"]:
        raise ValueError(f"Source revision mismatch: {path}")


def archive_members(data):
    """Read regular GNU/BSD ar members; reject duplicate or malformed names."""
    if not data.startswith(b"!<arch>\n"):
        raise ValueError("Not a regular ar archive")
    members = {}
    names = b""
    offset = 8
    while offset < len(data):
        header = data[offset : offset + 60]
        if len(header) != 60 or header[58:] != b"`\n":
            raise ValueError("Malformed ar member header")
        size = int(header[48:58])
        name = header[:16].decode("ascii").strip()
        start = offset + 60
        body = data[start : start + size]
        if len(body) != size:
            raise ValueError("Truncated ar member")
        offset = start + size + size % 2
        if name == "//":
            names = body
            continue
        if name in ("/", "/SYM64/"):
            continue
        if name.startswith("#1/"):
            count = int(name[3:])
            name, body = body[:count].decode().rstrip("\0"), body[count:]
        elif name.startswith("/"):
            index = int(name[1:])
            end = names.find(b"/\n", index)
            if end < index:
                raise ValueError("Malformed ar long filename")
            name = names[index:end].decode()
        else:
            name = name.removesuffix("/")
        if not name or "/" in name or name in members:
            raise ValueError(f"Invalid or duplicate ar member: {name}")
        members[name] = body
    if offset != len(data):
        raise ValueError("Invalid archive size")
    return members


def symbols(data):
    """Read ARM ELF32 symbol contracts without a host Python dependency."""
    if data[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError("Runtime member must be little-endian ELF32")
    header = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    if header[0] != 1 or header[1] != 40:
        raise ValueError("Runtime member must be a relocatable ARM object")
    section_offset, flags = header[5], header[6]
    section_size, section_count = header[10], header[11]
    if (
        section_size != 40
        or section_offset + section_size * section_count > len(data)
    ):
        raise ValueError("Invalid ELF section table")
    sections = [
        struct.unpack_from("<IIIIIIIIII", data, section_offset + 40 * i)
        for i in range(section_count)
    ]
    defined, undefined, weak = {}, set(), set()
    for section in sections:
        if section[1] != 2:
            continue
        strings = sections[section[6]]
        table = data[strings[4] : strings[4] + strings[5]]
        if section[9] != 16 or section[5] % 16:
            raise ValueError("Invalid ELF symbol table")
        for index in range(section[4], section[4] + section[5], 16):
            name, _, size, info, other, shndx = struct.unpack_from(
                "<IIIBBH", data, index
            )
            binding, kind = info >> 4, info & 15
            if binding not in (1, 2):
                continue
            end = table.find(b"\0", name)
            if end < name:
                raise ValueError("Invalid ELF symbol name")
            name = table[name:end].decode()
            if not shndx:
                if binding == 1:
                    undefined.add(name)
                else:
                    weak.add(name)
            else:
                defined.setdefault(name, set()).add(
                    (binding, kind, other & 3, size if kind != 2 else None)
                )
    return flags, defined, undefined, weak


def abi(members):
    definitions, undefined, weak = {}, set(), set()
    for body in members.values():
        _, present, missing, missing_weak = symbols(body)
        for name, contracts in present.items():
            definitions.setdefault(name, set()).update(contracts)
        undefined.update(missing)
        weak.update(missing_weak)
    return (
        definitions,
        undefined - definitions.keys(),
        weak - definitions.keys(),
    )


def verify_archive(base, output, replaced, specification):
    before, after = archive_members(base), archive_members(output)
    if list(before) != list(after):
        raise ValueError("Runtime member names or order changed")
    if (
        len(set(replaced)) != len(replaced)
        or not set(replaced) <= before.keys()
    ):
        raise ValueError("Replacement object list does not match the SDK")
    changed = [name for name in before if before[name] != after[name]]
    if set(changed) != set(replaced):
        raise ValueError(f"Unexpected changed members: {changed}")
    bridge = specification["native_page_bridge"]
    if digest(after[bridge["object"]]) != bridge["sha256"]:
        raise ValueError("Native RChunk bridge changed")
    for name in replaced:
        if symbols(before[name])[0] != symbols(after[name])[0]:
            raise ValueError(f"ARM ELF ABI flags changed: {name}")
    old, old_imports, old_weak = abi(before)
    new, new_imports, new_weak = abi(after)
    removed, added = old.keys() - new.keys(), new.keys() - old.keys()
    if removed != {"__mi_page_map"} or added != {
        "_mi_page_map",
        "_mi_page_map_max_address",
    }:
        raise ValueError(
            f"Unexpected definition changes: removed={removed}, added={added}"
        )
    if any(old[name] != new[name] for name in old.keys() & new.keys()):
        raise ValueError("A shared symbol changed binding/type/visibility")
    if old_imports != new_imports or old_weak != new_weak:
        raise ValueError("External strong/weak import closure changed")
    return {
        "member_count": len(before),
        "replaced_objects": replaced,
        "unchanged_member_count": len(before) - len(replaced),
        "members": [
            {
                "name": name,
                "base_sha256": digest(before[name]),
                "output_sha256": digest(after[name]),
            }
            for name in before
        ],
        "removed_internal_symbols": sorted(removed),
        "added_internal_symbols": sorted(added),
        "common_symbol_contracts_unchanged": True,
        "external_strong_imports": sorted(new_imports),
        "external_strong_imports_unchanged": True,
        "external_weak_imports": sorted(new_weak),
        "external_weak_imports_unchanged": True,
        "arm_elf_flags_unchanged": True,
        "native_page_bridge_sha256": digest(after[bridge["object"]]),
    }


def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        dir=path.parent, delete=False
    ) as temporary:
        temporary.write(data)
        temporary.flush()
        os.fsync(temporary.fileno())
        name = temporary.name
    os.chmod(name, 0o644)
    os.replace(name, path)


def build(args, specification, work, log):
    sdk = args.sdk.resolve()
    if (
        specification["format"] != 1
        or specification["sdk_version"] != "0.3.0"
        or specification["target_arch"] != "armv6"
        or specification["budget_bytes"] != 67108864
    ):
        raise ValueError(
            "Unsupported runtime manifest format/version/architecture/budget"
        )
    if (sdk / "VERSION").read_text().strip() != specification["sdk_version"]:
        raise ValueError("Only the pinned SDK 0.3.0 is supported")
    target = sdk / specification["base_archive"]["path"]
    base_hash = specification["base_archive"]["sha256"]
    expected = specification.get("output_archive_sha256")
    if target.is_symlink() or not target.is_file():
        raise ValueError("Runtime archive must be a regular file")
    installed = target.read_bytes()
    installed_hash = digest(installed)
    backup = work / "base/libsymbian_guest_runtime.a"
    if installed_hash == base_hash:
        base = installed
        if backup.exists():
            checked_file(backup, base_hash)
        else:
            atomic_write(backup, base)
    elif expected and installed_hash == expected:
        base = checked_file(backup, base_hash)
    else:
        raise ValueError(
            "SDK runtime archive differs from the pinned base/output; refusing replacement"
        )

    platform = (
        args.platform_source.resolve()
        if args.platform_source
        else work / "source/symbian-platform"
    )
    upstream = (
        args.mimalloc_source.resolve()
        if args.mimalloc_source
        else work / "source/mimalloc"
    )
    checkout(platform, specification["platform"], log)
    checkout(upstream, specification["mimalloc"], log)
    source = work / "patched-source"
    for name, expected_hash in specification["platform"]["files"].items():
        content = subprocess.check_output(
            [
                "git",
                "-C",
                str(platform),
                "show",
                specification["platform"]["revision"] + ":" + name,
            ]
        )
        if digest(content) != expected_hash:
            raise ValueError(f"Pinned SDK source mismatch: {name}")
        atomic_write(source / name, content)
        if name == "cpp/symbian/runtime/mimalloc_port.h":
            atomic_write(work / "base/mimalloc_port.h", content)
    patch = OVERLAY / specification["patch"]
    checked_file(patch, specification["patch_sha256"])
    apply_runtime_patch(
        source, patch, specification["patched_port_sha256"], log
    )
    port = source / "cpp/symbian/runtime/mimalloc_port.h"
    installed_port = sdk / "host/include/symbian/runtime/mimalloc_port.h"
    if installed_port.is_symlink() or not installed_port.is_file():
        raise ValueError("Installed runtime port header must be a regular file")
    if digest(installed_port.read_bytes()) not in (
        specification["platform"]["files"][
            "cpp/symbian/runtime/mimalloc_port.h"
        ],
        digest(port.read_bytes()),
    ):
        raise ValueError("Installed mimalloc port header differs")
    for name, expected_hash in specification["mimalloc"]["headers"].items():
        checked_file(upstream / "include" / name, expected_hash)
        checked_file(sdk / "include" / name, expected_hash)
    source_files = []
    replaced = []
    for item in specification["mimalloc"]["sources"]:
        path = upstream / item["path"]
        checked_file(path, item["sha256"])
        source_files.append(path)
        replaced.append(item["object"])
    source_files.append(source / "cpp/symbian/runtime/mimalloc_prim.c")
    replaced.append("mimalloc_prim.c.obj")

    compiler = sdk / "bin/clang"
    version = subprocess.check_output([str(compiler), "--version"], text=True)
    if "clang version 23." not in version:
        raise ValueError("The pinned runtime requires SDK Clang 23")
    output = work / "install/libsymbian_guest_runtime.a"
    output.parent.mkdir(parents=True, exist_ok=True)
    build_dir = work / "build"
    build_dir.mkdir(exist_ok=True)
    common = [
        str(compiler),
        "--target=armv6-none-eabi",
        "-march=armv6",
        "-mthumb",
        "-mfloat-abi=soft",
        "-nostdinc",
        "-std=c11",
        "-Os",
        "-fshort-wchar",
        "-fno-pic",
        "-fvisibility=hidden",
        "-ffunction-sections",
        "-fdata-sections",
        "-fdeclspec",
        "-Wno-ignored-attributes",
        "-DMI_DEBUG=0",
        "-DNDEBUG=1",
        "-DMI_DEFAULT_ARENA_RESERVE=8192",
        "-DSYMBIAN_RUNTIME_MIMALLOC=1",
        "-DSYMBIAN_MIMALLOC_ADDRESS_BUDGET_BYTES=67108864",
        "-include",
        str(port),
        "-ffile-prefix-map=" + str(work) + "=/symbian-runtime",
        "-ffile-prefix-map=" + str(upstream) + "=/mimalloc",
        "-ffile-prefix-map=" + str(sdk) + "=/symbian-sdk",
    ]
    common += [
        "-D" + name
        for name in (
            "__SYMBIAN32__",
            "__EPOC32__",
            "__GCC32__",
            "__MARM__",
            "__MARM_ARMV5__",
            "__GCCV3__",
            "__EABI__",
            "_UNICODE",
            "__LONG_LONG_SUPPORTED",
            "_POSIX_C_SOURCE=200112L",
        )
    ]
    common += ["-I" + str(upstream / "include")]
    common += [
        "-I" + str(sdk / "include" / name)
        for name in (
            "",
            "config",
            "compiler",
            "platform",
            "native",
            "openc",
            "libm",
            "libc",
            "pthread",
            "posix4",
            "abseil",
        )
    ]
    commands = []
    objects = []
    canary = build_dir / "config-probe.c"
    atomic_write(
        canary,
        b"#include <mimalloc/types.h>\n"
        b'_Static_assert(sizeof(void*) == 4, "32-bit target");\n'
        b'_Static_assert(MI_FREE_USE_PAGEMAP == 1, "page map enabled");\n'
        b'_Static_assert(MI_PAGE_MAP_FLAT == 1, "flat page map");\n'
        b'_Static_assert(MI_ARENA_ALIGNMENT == 32768, "arena alignment");\n'
        b'_Static_assert(SYMBIAN_MIMALLOC_ADDRESS_BUDGET_BYTES == 67108864, "unchanged budget");\n',
    )
    run(common + ["-fsyntax-only", str(canary)], stdout=log, stderr=log)
    for path, member in zip(source_files, replaced):
        obj = build_dir / member
        command = common + ["-c", str(path), "-o", str(obj)]
        commands.append(command)
        run(command, stdout=log, stderr=log)
        objects.append(str(obj))
    atomic_write(output, base)
    run(
        [str(sdk / "bin/llvm-ar"), "rcsD", str(output), *objects],
        stdout=log,
        stderr=log,
    )
    result = output.read_bytes()
    verification = verify_archive(base, result, replaced, specification)
    readelf = shutil.which("readelf")
    if not readelf:
        raise ValueError(
            "readelf (binutils) is required for ARM attribute validation"
        )
    base_members = archive_members(base)
    attributes = {}
    for member in replaced:
        original = build_dir / "base" / member
        atomic_write(original, base_members[member])
        old_attributes = subprocess.check_output(
            [readelf, "-A", str(original)], text=True
        )
        new_attributes = subprocess.check_output(
            [readelf, "-A", str(build_dir / member)], text=True
        )
        normalize = lambda text: [
            line.strip()
            for line in text.splitlines()
            if "Tag_ABI_optimization_goals:" not in line
        ]
        if normalize(old_attributes) != normalize(new_attributes):
            raise ValueError(
                f"ARM attributes changed beyond optimization goal: {member}"
            )
        attributes[member] = {"base": old_attributes, "output": new_attributes}
    verification["arm_attributes"] = attributes
    verification["arm_attributes_unchanged_except_optimization_goal"] = True
    if expected and digest(result) != expected:
        raise ValueError(
            "Rebuilt archive differs from the pinned reproducible output"
        )
    if digest(target.read_bytes()) != installed_hash:
        raise ValueError(
            "Installed runtime changed concurrently; refusing installation"
        )
    verification.update(
        {
            "status": "VERIFIED",
            "sdk_version": specification["sdk_version"],
            "target_arch": "armv6",
            "budget_bytes": specification["budget_bytes"],
            "compiled_configuration_canary_passed": True,
            "base_archive_sha256": base_hash,
            "output_archive_sha256": digest(result),
            "output_archive": str(output),
            "configuration_manifest_sha256": digest(MANIFEST.read_bytes()),
            "script_sha256": digest(Path(__file__).read_bytes()),
            "compiler_version": version.strip(),
            "compiler_sha256": digest(compiler.read_bytes()),
            "compile_commands": commands,
            "installed": bool(args.install),
            "platform_revision": specification["platform"]["revision"],
            "mimalloc_revision": specification["mimalloc"]["revision"],
        }
    )
    if args.install:
        if not expected:
            raise ValueError("Installation requires a pinned output digest")
        atomic_write(target, result)
        atomic_write(installed_port, port.read_bytes())
    atomic_write(
        work / "verification.json",
        (json.dumps(verification, indent=2) + "\n").encode(),
    )
    print(
        f"SDK ARMv6 runtime verified: {len(replaced)} replaced / {verification['unchanged_member_count']} unchanged objects"
    )
    print(f"64 MiB budget retained; archive SHA256 {digest(result)}")
    print(
        "Installed verified archive"
        if args.install
        else f"Output only: {output}"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sdk", type=Path, default=os.environ.get("SYMBIAN_SDK_PREFIX")
    )
    parser.add_argument("--work", type=Path)
    parser.add_argument("--platform-source", type=Path)
    parser.add_argument("--mimalloc-source", type=Path)
    parser.add_argument(
        "--install",
        action="store_true",
        help="Install only after all archive/source/ABI checks pass",
    )
    args = parser.parse_args()
    if not args.sdk:
        parser.error("set SYMBIAN_SDK_PREFIX or pass --sdk")
    work_base = Path(
        os.environ.get(
            "SYMBIAN_WEBKIT_WORK", ROOT.parents[1] / "WebKitBuild/Symbian"
        )
    )
    work = (args.work or work_base / "sdk-runtime").resolve()
    work.mkdir(parents=True, exist_ok=True)
    try:
        with (work / ".build.lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            with (work / "build.log").open("w") as log:
                build(args, json.loads(MANIFEST.read_text()), work, log)
    except (
        OSError,
        ValueError,
        KeyError,
        subprocess.CalledProcessError,
    ) as error:
        print(
            f"SDK runtime rebuild failed: {error}; see {work / 'build.log'}",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
