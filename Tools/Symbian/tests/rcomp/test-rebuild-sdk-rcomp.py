#!/usr/bin/env python3
"""Wrapper failure contracts using local Git repositories; no SDK or network writes."""

import hashlib
import importlib.util
import io
import json
import os
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "rebuild_rcomp", ROOT / "scripts/rebuild-sdk-rcomp.py"
)
rebuild = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(rebuild)


class RebuildContracts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="rcomp wrapper ")
        self.root = Path(self.temporary.name)
        self.outer = self.root / "outer"
        self.outer.mkdir()
        rebuild.run(["git", "init", "-q", self.outer])
        self.source = self.root / "upstream"
        self.source.mkdir()
        rebuild.run(["git", "init", "-q", self.source])
        self.source_file = self.source / "bintools/rcomp/src/pinned.cpp"
        self.source_file.parent.mkdir(parents=True)
        self.source_file.write_bytes(b"// pinned source\n")
        rebuild.run(["git", "-C", self.source, "add", "bintools"])
        rebuild.run(
            [
                "git",
                "-C",
                self.source,
                "-c",
                "user.name=Fixture",
                "-c",
                "user.email=fixture@example.invalid",
                "commit",
                "-qm",
                "fixture source",
            ]
        )
        self.revision = rebuild.git(self.source, "rev-parse", "HEAD").decode().strip()
        self.record = {
            "rcomp_revision": self.revision,
            "rcomp_repository": str(self.source),
        }

    def tearDown(self):
        self.temporary.cleanup()

    def contents(self, archive):
        with tarfile.open(fileobj=io.BytesIO(archive)) as package:
            return package.extractfile("bintools/rcomp/src/pinned.cpp").read()

    def test_interrupted_managed_initialization_recovers_inside_outer_git(self):
        managed = self.outer / "build/source"
        managed.mkdir(parents=True)
        rebuild.run(["git", "init", "-q", managed])  # Crash point before adding origin.
        outside = (self.outer / ".git/config").read_bytes()
        archive = rebuild.prepare_source(managed, self.record)
        self.assertEqual(self.contents(archive), b"// pinned source\n")
        self.assertEqual(
            rebuild.git(managed, "remote", "get-url", "origin").decode().strip(),
            str(self.source),
        )
        self.assertEqual(
            rebuild.git(managed, "config", "remote.origin.promisor").strip(), b"true"
        )
        self.assertEqual(
            rebuild.git(managed, "config", "remote.origin.partialclonefilter").strip(),
            b"blob:none",
        )
        self.assertEqual((self.outer / ".git/config").read_bytes(), outside)
        self.assertEqual(rebuild.prepare_source(managed, self.record), archive)

    def test_unknown_managed_origin_rejected_without_rewriting_it(self):
        managed = self.outer / "source"
        managed.mkdir()
        rebuild.run(["git", "init", "-q", managed])
        rebuild.run(
            ["git", "-C", managed, "remote", "add", "origin", str(self.root / "wrong")]
        )
        before = (managed / ".git/config").read_bytes()
        with self.assertRaisesRegex(
            SystemExit, "Unexpected cached rcomp source remote"
        ):
            rebuild.prepare_source(managed, self.record)
        self.assertEqual((managed / ".git/config").read_bytes(), before)

    def test_explicit_source_uses_pinned_objects_and_does_not_modify_config(self):
        self.source_file.write_bytes(b"// uncommitted work must not be compiled\n")
        before = (self.source / ".git/config").read_bytes()
        with patch.dict(
            os.environ,
            {"GIT_DIR": str(self.outer / ".git"), "GIT_WORK_TREE": str(self.outer)},
        ):
            archive = rebuild.prepare_source(self.source, self.record, managed=False)
        self.assertEqual(self.contents(archive), b"// pinned source\n")
        self.assertEqual(
            self.source_file.read_bytes(), b"// uncommitted work must not be compiled\n"
        )
        self.assertEqual((self.source / ".git/config").read_bytes(), before)

    def test_unknown_installed_tool_rejected_before_source_or_build(self):
        sdk = self.root / "synthetic-sdk"
        (sdk / "bin").mkdir(parents=True)
        (sdk / "bin/uidcrc").write_bytes(b"known synthetic uidcrc")
        tool = sdk / "bin/rcomp"
        tool.write_bytes(b"unknown tool must not be executed or replaced")
        manifest = self.root / "manifest"
        manifest.mkdir()
        record = {
            **self.record,
            "uidcrc_sha256": hashlib.sha256(
                (sdk / "bin/uidcrc").read_bytes()
            ).hexdigest(),
            "stock_rcomp_sha256": hashlib.sha256(b"known synthetic stock").hexdigest(),
        }
        (manifest / "manifest.json").write_text(json.dumps(record))
        before = tool.read_bytes()
        arguments = [
            "rebuild-sdk-rcomp.py",
            "--sdk",
            str(sdk),
            "--work-dir",
            str(self.root / "work"),
            "--install",
        ]
        with (
            patch.object(rebuild, "PATCHES", manifest),
            patch("sys.argv", arguments),
            patch.object(
                rebuild,
                "prepare_source",
                side_effect=AssertionError("source lookup must not occur"),
            ),
            self.assertRaisesRegex(SystemExit, "Unsupported rcomp bytes"),
        ):
            rebuild.main()
        self.assertEqual(tool.read_bytes(), before)
        self.assertFalse((self.root / "work/base").exists())


if __name__ == "__main__":
    unittest.main()
