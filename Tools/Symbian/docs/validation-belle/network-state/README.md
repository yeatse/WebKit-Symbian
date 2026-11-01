# Online/offline bridge: native IPC and browser validation

The production native observer passes 24 ASan/UBSan assertions; the actual EKA
connection registry passes 19 host assertions. Both modified EKA service
translation units compile independently. Patch `0020` replays onto the five
original files byte-for-byte. The [test sources and commands](../../../tests/network-state/README.md)
and [protocol provenance](../../../tests/network-state/provenance.json) are
maintained alongside the port.

The ARM E32 probe `55bd4d154d4d6853d2260c19ba3413689f648fea1dded83bca15064c7d7975a9`
has 19 checks. On the unchanged emulator
`71cbce0aa8a8eff9d167614a34f2870d26387df18b3940cf4818a34524933093`, it completes
with **6 PASS, 13 FAIL, guest exit 1, host exit 0, no panic or forced cleanup**.
The old fixed connection count/status and missing event/IPC implementation
cannot satisfy the real Start/Attach/Stop/reconnect expectations. The observer's
destruction check passes. `old-host-control/` preserves this failed runtime
result, not a feature PASS.

`failed-native-delete/` preserves the earlier image `99f26b90...` failure:
after 18 labels, observer teardown entered portable mimalloc with a native
`CBase` allocation and panicked. The bridge now pairs both locally owned
`Observer` and `RefreshTimer` with explicit `User::Free`, including construction
leaves. The mock allocator was also corrected to model the separate native
heap, so ASan plus the explicit free-count assertion catches this defect.
Imported `CActiveScheduler` and `CTrapCleanup` use their imported virtual
deleting destructors; the corrected guest run also exercises their cleanup.

On rebuilt host `046bc411...`, the same native image completes all **19/19**
checks. The first normal run then ends with host SIGSEGV (-11), so it remains
**FAIL** despite guest exit 0. A GDB attempt does not reproduce the crash:
the inferior exits normally, but the diagnostic script itself exits 1 when
trying to read registers afterwards; it is not a strict acceptance run.
A separate ordinary strict retry passes 19/19 with guest/host exit 0, no panic
and no forced cleanup. All three records are preserved in `native-after-0020/`.
The intermittent host exit failure is unresolved; the retry does not erase it.

Browser trusted online/offline event execution is still pending. The separate
controller, HTML and log validator are ready; [converter0005 evidence](converter-network-control/README.md)
records its explicit NetworkControl capability. Connection state is not
internet reachability, and the patch does not cancel already-open host
transport sockets or discover physical host radio/NIC changes.

`manifest.json` records binary identities without storing binaries, plus the
hashes of these logs and verification records. Original runtime directories
are `/workspace/symbian-online-work/runtime-before/` and
`/workspace/symbian-online-work/runtime-before-native-delete-fix/`.
