# EKA file-name cache: independent failure and candidate repair

The native OpenC/production copy probe passed 10/11 checks on the old host. The
failure was a differently cased source alias. The complete original guest log,
result and lossless emulator log are retained in `native-before/`; SHA-256
identities are in `raw-index.json`. The original FAIL remains unchanged.

Two distinct conditions were found. EFsrv already checks `io->exist` and returns
KErrAlreadyExists for exclusive creation of an existing file. However, the test
profile contained **both** host `Data/WebKit` and `Data/webkit` directories; the
resolver's exact-case preference selected different directories for the two
guest spellings. The candidate does not merge or delete existing host data to
hide that ambiguous layout.

Separately, a fresh collision-free directory reproduces a production cache
defect. EKA caches folded names using the directory's second-resolution mtime.
`common::open_c_file` did not invalidate that cache after creating a new file,
so a preceding negative lookup survived same-second `wb` or `ab` creation. An
explicit cache invalidation immediately made the same alias resolve correctly.

`candidate.patch` narrowly invalidates the index after a successful creating
open. It leaves ordinary truncation of an already existing file alone. The
actual old/new `fileutils.cpp` were compiled with the configured EKA compiler
command into private archives. `host-regression.json` records commands (including private dependency output), hashes
and outcomes: old `wb`/`ab` alias failures reproduced; repaired aliases found;
source bytes, failed-open behavior and both colliding directories preserved.

Patch [0023](../../../patches/eka2l1/0023-Invalidate-case-fold-cache-after-file-creation.patch)
is now applied to the EKA source, with the current `046bc411…` host executable
unchanged. `chain-0023.json` records a clean replay of all 19 root patches through
0023 from `5bbc0435…`, plus patch 0019 separately at the pinned Dynarmic revision;
the repaired production file matches the replay byte for byte. **Guest acceptance
is still pending.** The next native run must use a fresh, unique fixture directory
while retaining the case-variant assertion and all other exclusive-copy checks.

See the [reproduction test](../../../tests/eka-case-cache/README.md). This is a
host VFS correction; no WebKit path lowercasing or weakened copy check is used.
