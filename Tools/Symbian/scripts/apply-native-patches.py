#!/usr/bin/env python3
"""Apply the native SDK patch series without resetting a cached source tree.

Every prefix is computed from the pinned upstream commit in a temporary Git
index/object store. Only an exact prefix across all patch-owned files can be
upgraded. Other files and the caller's index are never written. Unknown edits
to a patch-owned file are rejected rather than guessed or overwritten.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile


class StackError(RuntimeError):
    """The source tree cannot safely be recognized or updated."""


def git(directory, *arguments, input_data=None):
    """Run Git in the selected repository, independent of caller Git overrides."""
    environment = os.environ.copy()
    for name in (
        "GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_OBJECT_DIRECTORY",
        "GIT_ALTERNATE_OBJECT_DIRECTORIES",
    ):
        environment.pop(name, None)
    result = subprocess.run(
        ["git", "-C", str(directory), *arguments], input=input_data,
        capture_output=True, env=environment,
    )
    if result.returncode:
        message = result.stderr.decode("utf-8", errors="replace").strip()
        raise StackError(message or "Git command failed")
    return result.stdout


def tree_entries(repository, tree, paths):
    result = {}
    output = git(repository, "ls-tree", "-r", "-z", tree, "--", *paths)
    for record in output.split(b"\0"):
        if not record:
            continue
        metadata, path = record.split(b"\t", 1)
        mode, kind, blob = metadata.decode("ascii").split()
        if kind != "blob" or mode not in ("100644", "100755"):
            raise StackError("Native patch stack requires regular source files")
        result[path.decode("utf-8")] = (mode, blob)
    return result


def worktree_entries(repository, paths):
    """Fingerprint owned files without reading or modifying unrelated files."""
    result = {}
    for name in paths:
        relative = Path(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise StackError("Patch path escapes the source tree")
        target = repository / relative
        ancestor = target
        while ancestor != repository:
            if ancestor.is_symlink():
                raise StackError("Symlink in native patch path: " + name)
            ancestor = ancestor.parent
        try:
            metadata = target.stat()
        except FileNotFoundError:
            continue
        if not stat.S_ISREG(metadata.st_mode):
            raise StackError("Native patch path is not a regular file: " + name)
        mode = "100755" if metadata.st_mode & 0o111 else "100644"
        blob = git(
            repository, "hash-object", "--no-filters", "--", name
        ).decode("ascii").strip()
        result[name] = (mode, blob)
    return result


def apply_stack(source, patch_directory, base):
    source = source.resolve()
    top = Path(git(source, "rev-parse", "--show-toplevel").decode().strip())
    if top.resolve() != source:
        raise StackError("--source must be the Git worktree root")
    base_commit = git(
        source, "rev-parse", "--verify", "--end-of-options", base + "^{commit}"
    ).decode("ascii").strip()
    patches = sorted(patch_directory.resolve().glob("*.patch"))
    if not patches or any(not p.is_file() or p.is_symlink() for p in patches):
        raise StackError("A regular, nonempty native patch series is required")
    patch_bytes = [p.read_bytes() for p in patches]
    object_format = git(source, "rev-parse", "--show-object-format").decode().strip()
    objects = Path(git(source, "rev-parse", "--git-path", "objects").decode().strip())
    if not objects.is_absolute():
        objects = source / objects
    if "\n" in str(objects):
        raise StackError("Unsupported source object-directory path")

    with tempfile.TemporaryDirectory(prefix="symbian-native-patches-") as temporary:
        scratch = Path(temporary)
        git(scratch, "init", "--quiet", "--object-format=" + object_format)
        alternate = scratch / ".git/objects/info/alternates"
        alternate.parent.mkdir(exist_ok=True)
        alternate.write_text(str(objects.resolve()) + "\n")
        git(scratch, "read-tree", base_commit)
        trees = [git(scratch, "write-tree").decode().strip()]
        owned = set()
        for patch, contents in zip(patches, patch_bytes):
            try:
                git(scratch, "apply", "--cached", "--whitespace=nowarn", "-",
                    input_data=contents)
            except StackError as error:
                raise StackError("Invalid native patch series at " + patch.name + ": " + str(error)) from error
            trees.append(git(scratch, "write-tree").decode().strip())
            changed = git(scratch, "diff", "--cached", "--name-only", "--no-renames", "-z", base_commit)
            owned.update(p.decode("utf-8") for p in changed.split(b"\0") if p)
        paths = sorted(owned)
        if not paths:
            raise StackError("Native patch series has no source changes")
        expected = [tree_entries(scratch, tree, paths) for tree in trees]
        actual = worktree_entries(source, paths)
        prefixes = [index for index, state in enumerate(expected) if state == actual]
        if not prefixes:
            differences = [name for name in paths if actual.get(name) != expected[-1].get(name)]
            raise StackError("Unknown/partial native patch state; no files changed. Check: " + ", ".join(differences))
        prefix = max(prefixes)
        if prefix != len(patches):
            pending = git(scratch, "diff", "--binary", "--no-renames", trees[prefix], trees[-1], "--", *paths)
            git(source, "apply", "--check", "--whitespace=nowarn", "-", input_data=pending)
            if worktree_entries(source, paths) != actual:
                raise StackError("Native source changed during validation; no files changed")
            git(source, "apply", "--whitespace=nowarn", "-", input_data=pending)
        if worktree_entries(source, paths) != expected[-1]:
            raise StackError("Native patch postimage verification failed")
        return {
            "status": "ALREADY_APPLIED" if prefix == len(patches) else "APPLIED",
            "base_commit": base_commit, "from_prefix": prefix,
            "patch_count": len(patches), "verified_files": len(paths),
            "patches": [
                {"name": p.name, "sha256": hashlib.sha256(data).hexdigest()}
                for p, data in zip(patches, patch_bytes)
            ],
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--patch-dir", type=Path, required=True)
    parser.add_argument("--base", default="v0.3.0")
    args = parser.parse_args()
    try:
        print(json.dumps(apply_stack(args.source, args.patch_dir, args.base), indent=2))
        return 0
    except (OSError, UnicodeError, StackError) as error:
        print("Native patch stack: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
