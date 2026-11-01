# Native IDB copy regression

This small E32 probe compiles the exact `copy` function extracted from
`Source/WTF/wtf/symbian/FileSystemSymbian.cpp`, using actual OpenC file APIs.
Only WTF path string storage is replaced with `std::string`; the copy loop,
open flags, metadata checks and cleanup remain production code. The generated
`production-source.json` binds the source and extracted function hashes.

The OpenC backend does not supply inode identity for ordinary files. The
probe records two distinct same-drive files' real `st_dev`/`st_ino` values.
Successful `O_EXCL` creation proves a new destination cannot alias the open
source, so that path must copy successfully even when both inodes are zero.
The overwrite path deliberately retains its conservative alias guard.

Build with `SYMBIAN_SDK_PREFIX`, `SYMBIAN_NATIVE_CONVERTER` and an isolated
`PROBE_BUILD`, then run `bash build.sh`. Run the image without arguments using
the sole emulator owner's strict launcher. It has UID `0xe0000904` and no
capabilities. Success requires **11/11**, the `idb-copy-probe exiting 0` marker,
normal guest and host exits, and no panic/access violation. Use a 30-second
watchdog. The fresh guest log is `C:/Data/WebKit/idb-copy-probe.log`.

Coverage includes exact binary payloads, existing targets, same-path and
case-insensitive source aliases, the retained overwrite guard, injected
partial-write ENOSPC cleanup, and successful reuse after rollback cleanup.
Version 2 creates one unique root-level `C:/IDBCopy-<pid>-<counter>/` directory
and its four fixture files, then removes them. The alias check changes the case
of the complete guest path. This avoids an existing invalid host profile with
both `Data/WebKit` and `Data/webkit`; it does not weaken case-insensitive behavior.
Version 1 and its original 10/11 failure remain archived separately. The browser's IndexedDB files and native clipboard are not touched.
This probe does not replace the three-launch 12/3/1 browser quota fixture.
