# File-sharing and read-only contracts

Status: **115 host checks and the 31-check native clipboard run passed**.

The [original native clipboard run](../clipboard-adapter/native-before/README.md)
passed 28/31 and failed the three read-only/exclusive-open checks. Its original
failure, guest exit 1 and host exit -11 remain unchanged. This directory does
not attribute the host shutdown failure to the file-service defect.

[Patch 0025](../../../patches/eka2l1/0025-Enforce-file-sharing-and-read-only-attributes.patch)
applies after 0024. It enforces sharing for independent file objects even within
one process, permits compatible readers, and releases a claim only after the
last duplicate/mapping reference closes. A denied open is checked before
truncation. Read-only state is persisted through the physical VFS; RFile SetAtt
is applied on flush/close. Unsupported attribute or timestamp updates return an
error. Output failures discard newly acquired handles and temporary files.
File-server sessions are cleared before their shared attribute state is destroyed.

An exclusively opened writable RFile renames its existing physical handle
without closing/reopening it, preserving data, cursor and attributes. RFs
Rename permits a read-only source; Delete does not. RFs Replace permits a
read-only source but rejects a read-only destination. These are distinct native
contracts, not a blanket refusal to rename a read-only file.

The [host result](host/result.json) records every source/header and linked
archive hash, exact compile/link arguments and the executable hash. The
[portable fixture](../../../tests/eka-file-contract/README.md) executes the
production sharing/attribute helpers and actual physical VFS against an isolated
real directory. It covers the 5-by-5 sharing matrix, promotion/demotion, count
release, attribute parsing, persistence after VFS reconstruction, denied-write
content preservation, and successful/failed open-handle rename preservation.
The complete modified files.cpp, fs.cpp and vfs.cpp translation units compiled.
Their full compiler output and probe output are retained under `host/`.

The host checks do **not** execute the file-server IPC handlers or prove guest
handle duplication or output failure rollback. The unchanged native clipboard
probe subsequently passed all 31 checks on rebuilt host `0d271449...`, including
the real read-only and exclusive-handle operations, with guest/host exit zero.
See the [native result](../clipboard-adapter/README.md). Android
content URI rename is unsupported by this change and has no copy/delete fallback;
Windows open-file rename is untested. Hidden/system/archive changes and explicit
timestamp updates remain unsupported. Existing generic RFs Replace deletion/
rename behavior is not made transactional by this patch.

[Patch replay](patch-replay.json) starts from EKA
`5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a`, checks/applies all 21 root patches
through 0025, and separately checks/applies 0019 to the pinned Dynarmic base.
All eight owned source files match the replay byte for byte.
[Source identities](source-manifest.json) preserve the exact before/after hashes.
An independent read-only review by sdk_setup checked sharing, final-reference
release, server teardown, output rollback and the official rename/attribute
semantics. Its final review found no remaining blocking issue in this scope.

The native contracts were checked against the pinned Symbian source revision
`0c3208650587ac0230aed8a74e9bddb5288023eb`; [official-sources.json](official-sources.json)
records each downloaded file URL and SHA-256. Relevant definitions are
`f32file.h` sharing promotion table; `cl_cli.cpp` RFs Delete/Rename/Replace and
SetAtt/SetEntry; `cl_file.cpp` RFile SetAtt/Rename; and `sf_file.cpp` exclusive
write-mode and cross-drive KErrBadName checks. The installed SDK header was used
for the public sharing matrix.
