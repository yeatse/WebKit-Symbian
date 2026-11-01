# IndexedDB copy and large-file failures

These records are failed controls and diagnosis. They do not establish a
successful browser quota suite; the final 12/3/1 independent-launch run is still
required.

| Run | Actual outcome | Identified issue |
| --- | --- | --- |
| Candidate 9 | Write 6/12; driver failure | New exclusive copies were rejected because OpenC reports no inode identity. |
| Candidate 10 | Write 10/12; driver failure | Copy fix reached Blob storage; oversized Blob staging was corrupted by the emulator, and the parallel fixture overwrote its first quota error. |
| Native copy version 1 | 10/11; guest 1, host 0 | Real same-drive inode values were zero; exclusive copy and rollback passed. Case alias failed in a host profile containing conflicting directory spellings. |
| Native large-file negative | 6/8; guest 1, host 0 | 37 MiB in 1 MiB writes round-tripped. One 34 MiB write falsely reported success, physically stored 4,622,576 bytes, and first differed at exactly 2 MiB. |

`artifacts.json` records source and archived byte hashes. Guest logs are raw or
losslessly gzipped. Browser result JSON changes only the absolute diagnostic log
path to a relative name. Image, emulator, fixture and media DLL identities are
retained in the results. No database payload or corrupt native output is stored
here: the old emulator could read arbitrary host bytes past its allocation.

The OpenC backend's `CFileDesc::FStat` and `MapStat` do not assign `st_ino` (official
`fdesc.cpp` blob `e61d4f7d0e7dc36f39de8fe91860626bdac3ceae`). Successfully creating
an output with `O_EXCL` proves that it cannot be the already open input, so the
production fix skips the unreliable inode comparison on this branch. The
existing overwrite branch remains conservative. The native copy log directly
confirms `st_dev=2, st_ino=0` for two distinct files. Host tests also inject zero
inodes, partial writes and cleanup failure paths against the actual source.

Candidate 10's supposedly 34 MiB Blob had correct JavaScript size metadata but a
5,059,824-byte sidecar. The metadata manifest preserves its length and hash only.
The first 2 MiB matched the input; later bytes did not. EKA's descriptor helper
used a 24-bit mask where the native ABI stores a 28-bit length. Thus 34 MiB became
2 MiB. The file server copied those 2 MiB into a host string, then passed the
independent 34 MiB request length to `fwrite`, ignored a partial write, and
reported success. The standalone negative control confirms this failure without
WebKit. The repair belongs to the emulator descriptor and file IPC boundaries,
not a smaller quota or a WebKit write-chunk workaround.

The separate two-case diagnostic preserved the first simultaneous-transaction
error: `QuotaExceededError` after accepted counts `[16,15]`. Explicitly aborting
the peer then generated `AbortError`, overwriting the fixture's error variable.
Both transactions rolled back to zero records. The fixture now retains the first
error and includes `String(error)` alongside the stack. Its original quota
thresholds are unchanged. A separate 20 MiB Blob stored on the second origin is read byte-for-byte after
a full process restart. Both `Blob.size` and same-process reads can miss a short
sidecar: the registry may reuse the original in-memory Blob URL. The original
24 MiB array baseline and all quota thresholds remain unchanged.

`diagnostic-two-case/` is a bounded diagnostic page, not the full acceptance
suite. Serve it on a fresh origin, pass a unique `?run=...` token and inspect its
`IDB_DIAG_RESULT`; it removes only its own three databases. The full browser
suite lives in `tests/idb-quota`, the native copy probe in `tests/native-idb-copy`,
and the boundary probe in `tests/native-large-file`. Preserve these failed
records when adding later successful runs.
