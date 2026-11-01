# 28-bit descriptor lengths and bounded file IPC

The saved original EKA source fails the actual-source host regression at the
first 16 MiB descriptor boundary. The final 0024 candidate passes **299 checks
under AddressSanitizer and UndefinedBehaviorSanitizer**, including an entire
34 MiB write/read comparison through a real private host file. Both affected
compiler units pass the configured EKA compiler command with `-fsyntax-only`.
These are host results; guest and IndexedDB quota acceptance remain separate.

The SDK defines the descriptor length mask as `0x0fffffff` and type shift as 28.
Its header hashes are retained. EKA's old 24-bit getter converted a 34 MiB length
into 2 MiB. The file server materialized that truncated length, passed the
independent 34 MiB count to `fwrite` and ignored its returned byte count. The
original corrupted Blob and native negative-control evidence remain in
[quota-copy](../quota-copy/README.md). No quota threshold or WebKit producer
expectation was relaxed.

Patch [0024](../../../patches/eka2l1/0024-Preserve-descriptor-lengths-and-bound-file-IPC-transfers.patch)
corrects the packed descriptor length helpers. The file server preflights the
complete descriptor/header/payload mappings and permissions, checks arguments
and positions, and copies through a 64 KiB staging buffer across guest pages.
It accepts legal guest-contiguous buffers with noncontiguous host backing.
Short writes, invalid mappings, seek failures and unexpected short reads return
errors; EOF succeeds with a shortened descriptor. The 64-bit position packet is
also read through the checked page-aware path.

Independent SDK review corrected two inherited/initial expectations before
application: oversized Read returns `KErrOverflow`, and positioned Write beyond
EOF starts at EOF. The official `f32file64.inl` documents both. OpenC's official
`fdesc.cpp` (`e61d4f7d0e7dc36f39de8fe91860626bdac3ceae`, lines 997–1018) calls
`iFile.SetSize(iPos)` before `iFile.Write(iPos, ptr)` when it needs a POSIX hole.
The unsafe native gap-fill branch is removed. `initial299-*` retains the earlier
candidate's different source/harness identities and is not final acceptance.

The [test README](../../../tests/eka-file-io/README.md) defines contracts and the
reproduction command. Logs are copied unchanged. Result metadata retains source,
runner and harness hashes; only command path prefixes are normalized. The old
source exits 1 at check 16; the final candidate exits 0. `chain-result.json`
records all 20 root patches through 0024 applied to an isolated index at
`5bbc0435…`; all three files match the tested and applied source byte for byte.
0019 targets the separately pinned Dynarmic repository, outside that root index.
`application-result.json` records strict before/after SHA verification. No EKA
commit or push was made. The normal host build and MMF asset restoration are
handled separately from these host function tests.

The normal host build subsequently completed: executable SHA-256
`3c4e27c486b696171502ff28b5577bbed48666c238c2ff85b335d79a0083467c`,
65,473,264 bytes. The established audio/video DLLs were restored and both DLL
and both map-file hashes matched. `host-identities.json` records all four assets;
`host-build.log.gz` is the unchanged complete build log compressed losslessly.
No guest or browser acceptance run was performed as part of this build.
