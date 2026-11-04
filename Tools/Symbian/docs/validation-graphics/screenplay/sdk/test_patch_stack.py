#!/usr/bin/env python3
"""Exercise the real native patch stack on a sparse upstream source fixture."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest


def command(directory, *arguments, check=True):
    return subprocess.run(
        list(arguments), cwd=directory, capture_output=True, check=check
    )


def git(directory, *arguments):
    return command(directory, "git", *arguments).stdout


def snapshot(directory, names):
    result = {}
    for name in names:
        path = directory / name
        if path.is_symlink():
            result[name] = ("symlink", os.readlink(path))
        elif path.exists():
            result[name] = (
                stat.S_IMODE(path.stat().st_mode), path.read_bytes()
            )
        else:
            result[name] = None
    return result


class NativePatchStackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="native-stack-tests-")
        cls.root = Path(cls.temporary.name)
        cls.base = cls.root / "baseline"
        cls.base.mkdir()
        git(cls.base, "init", "--quiet")
        cls.patches = sorted(ARGS.patch_dir.resolve().glob("*.patch"))
        assert len(cls.patches) == 10, "Tests pin original nine plus ScreenPlay0010"
        names = set()
        for patch in cls.patches:
            for record in git(cls.base, "apply", "--numstat", "-z", str(patch)).split(b"\0"):
                if record:
                    names.add(record.split(b"\t", 2)[2].decode())
        cls.names = sorted(names)
        cls.manifest = {
            "upstream_commit": git(ARGS.upstream, "rev-parse", "v0.3.0^{commit}").decode().strip(),
            "files": [],
            "patches": [
                {"name": p.name, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                for p in cls.patches
            ],
        }
        for name in cls.names:
            result = command(ARGS.upstream, "git", "show", "v0.3.0:" + name, check=False)
            entry = {"path": name, "present_in_upstream": result.returncode == 0}
            if result.returncode == 0:
                target = cls.base / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(result.stdout)
                mode = git(ARGS.upstream, "ls-tree", "v0.3.0", "--", name).split()[0]
                target.chmod(0o755 if mode == b"100755" else 0o644)
                entry["sha256"] = hashlib.sha256(result.stdout).hexdigest()
            cls.manifest["files"].append(entry)
        git(cls.base, "add", ".")
        git(cls.base, "-c", "user.name=Native patch regression", "-c", "user.email=fixture@example.invalid", "commit", "--quiet", "-m", "Sparse real v0.3.0 fixture")
        git(cls.base, "tag", "v0.3.0")
        cls.expected = cls.root / "expected"
        git(cls.root, "clone", "--quiet", "--shared", str(cls.base), str(cls.expected))
        for count, patch in enumerate(cls.patches, 1):
            git(cls.expected, "apply", str(patch))
            if count == 6:
                cls.sixth_final = snapshot(cls.expected, cls.names)
        cls.final = snapshot(cls.expected, cls.names)
        if ARGS.output:
            ARGS.output.mkdir(parents=True, exist_ok=True)
            (ARGS.output / "fixture-identities.json").write_text(json.dumps(cls.manifest, indent=2) + "\n")

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def setUp(self):
        self.directory = self.root / self.id().rsplit(".", 1)[-1]
        git(self.root, "clone", "--quiet", "--shared", str(self.base), str(self.directory))

    def prefix(self, count):
        for patch in self.patches[:count]:
            git(self.directory, "apply", str(patch))

    def helper(self, patches=None):
        return command(
            self.directory, sys.executable, str(ARGS.helper),
            "--source", str(self.directory), "--patch-dir",
            str(patches or ARGS.patch_dir), check=False,
        )

    def assert_no_write_failure(self, patches=None):
        before = snapshot(self.directory, self.names)
        index = (self.directory / ".git/index").read_bytes()
        result = self.helper(patches)
        self.assertNotEqual(result.returncode, 0, result.stdout.decode())
        self.assertEqual(snapshot(self.directory, self.names), before)
        self.assertEqual((self.directory / ".git/index").read_bytes(), index)

    def test_clean_and_second_invocation(self):
        index = (self.directory / ".git/index").read_bytes()
        first = self.helper()
        self.assertEqual(first.returncode, 0, first.stderr.decode())
        self.assertEqual(json.loads(first.stdout)["from_prefix"], 0)
        self.assertEqual(snapshot(self.directory, self.names), self.final)
        times = {name: (self.directory / name).stat().st_mtime_ns for name in self.names}
        second = self.helper()
        self.assertEqual(second.returncode, 0, second.stderr.decode())
        self.assertEqual(json.loads(second.stdout)["status"], "ALREADY_APPLIED")
        self.assertEqual({name: (self.directory / name).stat().st_mtime_ns for name in self.names}, times)
        self.assertEqual((self.directory / ".git/index").read_bytes(), index)

    def test_every_prefix_upgrades(self):
        for count in range(1, len(self.patches)):
            with self.subTest(prefix=count):
                work = self.root / ("prefix-" + str(count))
                git(self.root, "clone", "--quiet", "--shared", str(self.base), str(work))
                previous = self.directory
                self.directory = work
                self.prefix(count)
                index = (work / ".git/index").read_bytes()
                result = self.helper()
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual(json.loads(result.stdout)["from_prefix"], count)
                self.assertEqual(snapshot(work, self.names), self.final)
                self.assertEqual((work / ".git/index").read_bytes(), index)
                self.directory = previous

    def test_full_stack_early_reverse_check_is_not_a_completion_test(self):
        self.prefix(len(self.patches))
        old = command(self.directory, "git", "apply", "--reverse", "--check", str(self.patches[0]), check=False)
        self.assertNotEqual(old.returncode, 0)
        result = self.helper()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(json.loads(result.stdout)["status"], "ALREADY_APPLIED")

    def test_partial_sixth_patch_is_rejected_without_writes(self):
        self.prefix(5)
        name = "cpp/symbian/e32/e32.cc"
        (self.directory / name).write_bytes(self.sixth_final[name][1])
        self.assert_no_write_failure()

    def test_last_patch_present_does_not_hide_missing_earlier_state(self):
        self.prefix(len(self.patches))
        name = "cpp/symbian/e32/exports.cc"
        (self.directory / name).write_bytes((self.base / name).read_bytes())
        self.assert_no_write_failure()

    def test_original_six_series_still_idempotent(self):
        self.prefix(6)
        legacy = self.root / "legacy-six-patches"
        legacy.mkdir()
        for patch in self.patches[:6]:
            shutil.copy2(patch, legacy / patch.name)
        before = snapshot(self.directory, self.names)
        index = (self.directory / ".git/index").read_bytes()
        result = self.helper(legacy)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        report = json.loads(result.stdout)
        self.assertEqual(report["status"], "ALREADY_APPLIED")
        self.assertEqual(report["from_prefix"], 6)
        self.assertEqual(report["patch_count"], 6)
        self.assertEqual(snapshot(self.directory, self.names), before)
        self.assertEqual((self.directory / ".git/index").read_bytes(), index)

    def test_partial_math_patch_is_rejected_without_writes(self):
        self.prefix(6)
        name = "cpp/symbian/runtime/include/math.h"
        (self.directory / name).write_bytes((self.expected / name).read_bytes())
        self.assert_no_write_failure()

    def test_unknown_math_owner_edit_is_preserved(self):
        self.prefix(7)
        path = self.directory / "cpp/symbian/runtime/math_c.cc"
        with path.open("ab") as stream:
            stream.write(b"\n// User-owned math change.\n")
        self.assert_no_write_failure()

    def test_partial_mmf_patch_is_rejected_without_writes(self):
        self.prefix(7)
        name = "cpp/symbian/e32/e32.cc"
        (self.directory / name).write_bytes((self.expected / name).read_bytes())
        self.assert_no_write_failure()

    def test_partial_complex_owner_is_rejected_without_writes(self):
        self.prefix(8)
        name = "cpp/symbian/runtime/CMakeLists.txt"
        body = (self.expected / name).read_text()
        body = body.replace('-ffp-contract=off', '-ffp-contract=fast')
        (self.directory / name).write_text(body)
        self.assert_no_write_failure()

    def test_unknown_complex_owner_edit_is_preserved(self):
        self.prefix(9)
        path = self.directory / "cpp/symbian/runtime/CMakeLists.txt"
        with path.open("ab") as stream:
            stream.write(b"\n# User-owned complex runtime edit.\n")
        self.assert_no_write_failure()

    def test_original_eight_series_still_idempotent(self):
        self.prefix(8)
        legacy = self.root / "legacy-eight-patches"
        legacy.mkdir()
        for patch in self.patches[:8]:
            shutil.copy2(patch, legacy / patch.name)
        before = snapshot(self.directory, self.names)
        result = self.helper(legacy)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        report = json.loads(result.stdout)
        self.assertEqual(report["status"], "ALREADY_APPLIED")
        self.assertEqual(report["from_prefix"], 8)
        self.assertEqual(snapshot(self.directory, self.names), before)

    def test_original_nine_series_still_idempotent(self):
        self.prefix(9)
        legacy = self.root / "legacy-nine-patches"
        legacy.mkdir()
        for patch in self.patches[:9]:
            shutil.copy2(patch, legacy / patch.name)
        before = snapshot(self.directory, self.names)
        result = self.helper(legacy)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        report = json.loads(result.stdout)
        self.assertEqual(report["status"], "ALREADY_APPLIED")
        self.assertEqual(report["from_prefix"], 9)
        self.assertEqual(snapshot(self.directory, self.names), before)

    def test_partial_screenplay_patch_is_rejected_without_writes(self):
        self.prefix(9)
        name = "symbian/toolchain/cmake/SymbianScreenPlay.cmake"
        path = self.directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes((self.expected / name).read_bytes())
        self.assert_no_write_failure()

    def test_unknown_screenplay_manifest_edit_is_preserved(self):
        self.prefix(10)
        path = self.directory / "research/native-sdk/screenplay.json"
        with path.open("ab") as stream:
            stream.write(b"\nUser-owned source contract change.\n")
        self.assert_no_write_failure()

    def test_unknown_owned_file_edit_is_preserved(self):
        self.prefix(5)
        path = self.directory / "cpp/symbian/e32/e32.cc"
        with path.open("ab") as stream:
            stream.write(b"\n// User-owned local change, not a stack prefix.\n")
        self.assert_no_write_failure()

    def test_unrelated_staged_and_unstaged_user_edits_are_preserved(self):
        self.prefix(5)
        note = self.directory / "user-notes.txt"
        note.write_bytes(b"staged local note\n")
        git(self.directory, "add", "user-notes.txt")
        note.write_bytes(b"unstaged local note\n")
        index = (self.directory / ".git/index").read_bytes()
        result = self.helper()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(snapshot(self.directory, self.names), self.final)
        self.assertEqual(note.read_bytes(), b"unstaged local note\n")
        self.assertEqual((self.directory / ".git/index").read_bytes(), index)

    def test_unknown_added_file_collision_is_preserved(self):
        path = self.directory / "cpp/tests/e32_plugin_identity_test.cc"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"user file, not supplied by the stack\n")
        self.assert_no_write_failure()

    def test_missing_owned_file_is_rejected(self):
        self.prefix(6)
        (self.directory / "cpp/symbian/e32/e32.cc").unlink()
        self.assert_no_write_failure()

    def test_owned_symlink_is_rejected(self):
        self.prefix(5)
        path = self.directory / "cpp/symbian/e32/e32.cc"
        path.unlink()
        outside = self.root / "outside-user-file"
        outside.write_bytes(b"must remain untouched\n")
        path.symlink_to(outside)
        self.assert_no_write_failure()
        self.assertEqual(outside.read_bytes(), b"must remain untouched\n")

    def test_invalid_series_is_rejected_even_on_full_postimage(self):
        self.prefix(len(self.patches))
        patches = self.root / "broken-series"
        shutil.copytree(ARGS.patch_dir, patches)
        first = patches / self.patches[0].name
        lines = first.read_text().splitlines(keepends=True)
        index = next(i for i, line in enumerate(lines) if line.startswith(" ") and line.strip())
        lines[index] = " context-not-in-the-pinned-upstream\n"
        first.write_text("".join(lines))
        self.assert_no_write_failure(patches)


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path, required=True)
    parser.add_argument("--patch-dir", type=Path, default=root / "patches/symbian-native")
    parser.add_argument("--helper", type=Path, default=root / "scripts/apply-native-patches.py")
    parser.add_argument("--output", type=Path)
    ARGS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
