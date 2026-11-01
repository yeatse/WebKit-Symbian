# Native SDK patch-stack regression

`apply-native-patches.py` validates the ordered six-patch series against the
`v0.3.0` Git object in an isolated temporary index/object store. It recognizes
the complete contents and executable modes of every patch-owned file at each
prefix. A clean tree, a complete prefix, and the complete final tree are
supported. The final state is verified again after application.

No SDK reset/clean, real-index writes, or per-patch reverse-apply shortcut is
used. Unrelated staged/unstaged files are preserved. Unknown edits in a
patch-owned file, including partial or conflicting patches, are rejected before
writing anything; this helper deliberately does not merge those edits. The
caller should use its normal exclusive build-source directory.

`build-symbian-native.sh` calls the same helper before configuring the converter
and once more after build/tests. Thus a fresh CI cache miss also checks the
already-patched/idempotent path before packaging proceeds.

Run from the WebKit checkout with an existing upstream SDK Git repository:

```sh
python3 Tools/Symbian/tests/sdk-native-patch-stack/test_patch_stack.py \
  --upstream /path/to/symbian-platform --output /tmp/native-stack-checks -v
```

The test reads only `git show v0.3.0:PATH`, never the possibly modified SDK
worktree. It copies only files touched by the actual six patches into temporary
small Git repositories; new-file absence and upstream blob identities are
recorded in `fixture-identities.json`. It does not clone/build a second SDK,
run an emulator, or touch the original repository/cache.

Checks cover clean application, every prefix 1–5, full-stack repeat without
mtime/index changes, the former reverse-check failure, partial/unknown/missing
owned files, a final patch that cannot hide missing earlier changes, preserved
unrelated staged/unstaged edits, symlink refusal and an invalid series even when
the current worktree already has the old complete postimage.
