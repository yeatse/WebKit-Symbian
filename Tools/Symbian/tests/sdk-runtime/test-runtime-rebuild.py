#!/usr/bin/env python3
"""Fail-closed checks using the actual SDK baseline and rebuilt ARM archive."""

import argparse
import importlib.util
import io
import json
import os
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "runtime_rebuild", ROOT / "scripts/rebuild-sdk-runtime.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def pack(members):
    result = bytearray(b"!<arch>\n")
    for name, data in members.items():
        encoded = name.encode()
        body = encoded + data
        header = (
            f"{'#1/' + str(len(encoded)):<16}{0:<12}{0:<6}"
            f"{0:<6}{'100644':<8}{len(body):<10}`\n"
        )
        assert len(header) == 60
        result.extend(header.encode())
        result.extend(body)
        if len(body) % 2:
            result.extend(b"\n")
    return bytes(result)


class RuntimeChecks(unittest.TestCase):
    def check(self, result):
        return MODULE.verify_archive(
            self.base, result, self.replaced, self.manifest
        )

    def test_real_archive(self):
        report = self.check(self.output)
        self.assertEqual(report["unchanged_member_count"], 82)
        self.assertEqual(report["member_count"], 103)
        self.assertEqual(
            MODULE.digest(self.output), self.manifest["output_archive_sha256"]
        )

    def test_untouched_bridge_must_match(self):
        members = dict(self.members)
        name = self.manifest["native_page_bridge"]["object"]
        members[name] = members[name][:-1] + bytes([members[name][-1] ^ 1])
        with self.assertRaisesRegex(ValueError, "Unexpected changed"):
            self.check(pack(members))

    def test_missing_member(self):
        members = dict(self.members)
        del members["alloc.c.obj"]
        with self.assertRaisesRegex(ValueError, "names or order"):
            self.check(pack(members))

    def test_duplicate_member(self):
        archive = pack({"same.obj": b"first"})
        archive += pack({"same.obj": b"second"})[8:]
        with self.assertRaisesRegex(ValueError, "duplicate"):
            MODULE.archive_members(archive)

    def test_truncated_archive(self):
        with self.assertRaises(ValueError):
            MODULE.archive_members(self.output[:-17])

    def test_changed_abi_flags(self):
        members = dict(self.members)
        body = bytearray(members["alloc.c.obj"])
        struct.pack_into("<I", body, 36, 0x05000400)  # Hard-float PCS.
        members["alloc.c.obj"] = bytes(body)
        with self.assertRaisesRegex(ValueError, "ARM ELF ABI"):
            self.check(pack(members))

    def test_added_external_dependency(self):
        members = dict(self.members)
        # Rename an actual external import while retaining valid ELF layout.
        external = MODULE.abi(members)[1]
        for name in self.replaced:
            imports = MODULE.symbols(members[name])[2] & external
            if imports:
                symbol = min(imports)
                members[name] = members[name].replace(
                    symbol.encode() + b"\0",
                    ("x" + symbol[1:]).encode() + b"\0",
                )
                break
        else:
            self.fail("Expected pinned external dependency not found")
        with self.assertRaisesRegex(ValueError, "import closure"):
            self.check(pack(members))

    def test_unknown_internal_symbol_change(self):
        members = dict(self.members)
        name = "page-map.c.obj"
        self.assertTrue(b"_mi_page_map\0" in members[name])
        members[name] = members[name].replace(
            b"_mi_page_map\0", b"xmi_page_map\0"
        )
        with self.assertRaisesRegex(ValueError, "definition changes"):
            self.check(pack(members))

    def test_unsupported_sdk_version(self):
        with tempfile.TemporaryDirectory() as directory:
            sdk = Path(directory)
            (sdk / "VERSION").write_text("9.9.9\n")
            with self.assertRaisesRegex(ValueError, "Only the pinned"):
                MODULE.build(
                    SimpleNamespace(sdk=sdk),
                    self.manifest,
                    sdk / "work",
                    io.StringIO(),
                )

    def test_unrecognized_base_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            sdk = Path(directory)
            (sdk / "VERSION").write_text("0.3.0\n")
            archive = sdk / self.manifest["base_archive"]["path"]
            archive.parent.mkdir(parents=True)
            archive.write_bytes(b"not the release library")
            with self.assertRaisesRegex(ValueError, "pinned base/output"):
                MODULE.build(
                    SimpleNamespace(sdk=sdk),
                    self.manifest,
                    sdk / "work",
                    io.StringIO(),
                )

    def nested_patch(self, polluted_environment=False, wrong_hash=False):
        with tempfile.TemporaryDirectory() as directory:
            outer = Path(directory)
            subprocess.run(["git", "init", "-q", str(outer)], check=True)
            source = outer / "WebKitBuild/Symbian/sdk-runtime/patched-source"
            relative = Path("cpp/symbian/runtime/mimalloc_port.h")
            # The checked release header comes from the real source snapshot
            # retained by the production rebuild, not a simplified fake patch.
            before = self.original_port
            self.assertEqual(
                MODULE.digest(before),
                self.manifest["platform"]["files"][str(relative)],
            )
            for root in (outer, source):
                (root / relative).parent.mkdir(parents=True, exist_ok=True)
                (root / relative).write_bytes(before)
            environment = {}
            if polluted_environment:
                environment = {
                    "GIT_DIR": str(outer / ".git"),
                    "GIT_WORK_TREE": str(outer),
                    "GIT_INDEX_FILE": str(outer / ".git" / "foreign-index"),
                    "GIT_CONFIG_COUNT": "1",
                    "GIT_CONFIG_KEY_0": "core.worktree",
                    "GIT_CONFIG_VALUE_0": str(outer),
                }
            expected = (
                "0" * 64 if wrong_hash else self.manifest["patched_port_sha256"]
            )
            with (
                patch.dict(os.environ, environment),
                tempfile.TemporaryFile() as log,
            ):
                MODULE.apply_runtime_patch(
                    source,
                    MODULE.OVERLAY / self.manifest["patch"],
                    expected,
                    log,
                )
            self.assertEqual((outer / relative).read_bytes(), before)
            self.assertEqual(
                MODULE.digest((source / relative).read_bytes()), expected
            )
            self.assertFalse((outer / ".git" / "foreign-index").exists())

    def test_patch_inside_another_git_checkout(self):
        self.nested_patch()

    def test_patch_ignores_outer_git_environment(self):
        self.nested_patch(polluted_environment=True)

    def test_patch_exit_zero_without_expected_bytes_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
            self.nested_patch(wrong_hash=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", required=True, type=Path)
    args = parser.parse_args()
    RuntimeChecks.manifest = json.loads(MODULE.MANIFEST.read_text())
    RuntimeChecks.base = MODULE.checked_file(
        args.work / "base/libsymbian_guest_runtime.a",
        RuntimeChecks.manifest["base_archive"]["sha256"],
    )
    RuntimeChecks.output = MODULE.checked_file(
        args.work / "install/libsymbian_guest_runtime.a",
        RuntimeChecks.manifest["output_archive_sha256"],
    )
    RuntimeChecks.members = MODULE.archive_members(RuntimeChecks.output)
    RuntimeChecks.original_port = MODULE.checked_file(
        args.work / "base/mimalloc_port.h",
        RuntimeChecks.manifest["platform"]["files"][
            "cpp/symbian/runtime/mimalloc_port.h"
        ],
    )
    RuntimeChecks.replaced = [
        item["object"] for item in RuntimeChecks.manifest["mimalloc"]["sources"]
    ]
    RuntimeChecks.replaced.append("mimalloc_prim.c.obj")
    result = unittest.TextTestRunner(verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(RuntimeChecks)
    )
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
