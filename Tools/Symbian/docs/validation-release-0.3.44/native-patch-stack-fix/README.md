# CI44 native patch-stack cache repair

CI44 failed before producing SIS: the earlier per-patch reverse-check no longer recognized a complete overlapping 0001–0006 stack. The helper now constructs and validates all prefix postimages against v0.3.0 in a temporary Git index/object store, accepts only an exact known state across every patch-owned file, applies the remaining net diff and verifies the final image. It never resets the SDK tree or writes the caller index. Unknown edits to patch-owned files are refused and preserved; unrelated staged/unstaged edits remain unchanged.

Root applied the four-file patch to the main worktree at207c9995d51 and independently ran the real-stack regression:11 tests pass, including prefix1–5 subtests, clean/full-repeat, partial/conflicting/missing state, preserved local edits/index and symlink refusal. Shell syntax and owned-source diff checks pass. The build script calls the same helper again after build/tests, so a fresh CI45 cache miss also exercises idempotence. These checks do not claim a new converter build or CI45 SIS result.

The original CI44 failure report is retained in ../ci-failed. No SDK source/cache, guest or display was used for this repair validation. Source-file provenance and exact patch identities are in fixture-identities.json and candidate-manifest.json; root-tests.log is the actual root regression output.
