# IndexedDB quota: three independent browser launches

Candidate 10 passed **16/16** cases on 2026-10-29: 12 write cases, three
restart/read cases, and one deletion/restart case. Each browser driver exited
normally with status zero. The immutable HTTP reports agree with the fresh
per-case guest console output. `result.json` records the exact image, host and
fixture identities; `manifest.json` verifies the archived bytes and the original
bytes of each losslessly compressed log.

The write phase verifies transactional Blob replacement, abort and clear;
rejection of an oversized Blob while preserving its source and old record;
shared pending reservations between simultaneous databases; the 32 MiB origin
and 128 MiB port limits; origin isolation; and deletion returning quota. After a
full process restart, the browser reads both the original 24 MiB array baseline
and every byte of a separate 20 MiB Blob. This exercises the persisted Blob
file, rather than an in-memory Blob URL or its size metadata. The third launch
confirms deletion persisted and storage is writable again.

Browser SHA-256: `3a2385c1bd571d51c98f58f967a8ecf499ba1dcfb86d42d58b1e7dc82dd70317`.
Host SHA-256: `3c4e27c486b696171502ff28b5577bbed48666c238c2ff85b335d79a0083467c`.
The host includes patches through 0024 and the separately rebuilt native MMF
DLLs. `browser-identities.json` describes the first-batch image and its two quota
fix compilation units. The later, unvalidated second-batch browser features are
not included in this image.

`environment-start-failure/` preserves the preceding attempt that could not
connect to X display `:95`; it never entered the browser test. The successful
run used a fresh token after restoring the display service. Earlier development
failures remain separate from this successful result.

This does not test sudden power loss or fill the physical drive to its reserve.
The 16 MiB free-space admission check is not a system-wide disk reservation.
Blob staging occurs before quota admission, and copied Blob files are not
fsynced. No database, Blob contents or private profile data is archived here.
