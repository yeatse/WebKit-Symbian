# Emulator mapped-volume space

Patch `0018-Report-space-for-the-actual-mapped-filesystem-volume.patch` makes
`RFs::Volume` query the filesystem containing the actual VFS drive root. It uses
`get_raw_path(X:\\)`, including the selected firmware subdirectory for ROM,
and never substitutes the process's current directory when a mapping is absent.
`iSize` is the backing filesystem capacity; writable `iFree` is the space
available to the host user, excluding reserved blocks. Both retain signed
64-bit precision. ROM and write-protected mappings report zero writable bytes.
ROM capacity describes the emulator's **backing filesystem**, not the phone's
physical ROM or the firmware file's length.

Invalid drive numbers fail with `KErrArgument`, absent mounts/directories with
`KErrNotReady`, host permission errors with `KErrAccessDenied`, and values beyond
`TInt64` with `KErrOverflow`. `KDefaultDrive` follows the RFs session path.
Invalid session paths fail instead of indexing the drive table. An output
descriptor must hold the complete drive/UID/size/free/name prefix: 552 bytes
for EKA1 and 556 for EKA2. Older EKA2 clients may omit the trailing cache/reserved
extension. Null/invalid or smaller descriptors fail; structures are initialized
before copying to the guest.

Run the standalone host checks after applying the ordered emulator patches:

```sh
python3 Tools/Symbian/tests/native-volume/run-host-volume.py \
  --eka-root /path/to/EKA2L1 \
  --output /path/to/new-host-volume-results
```

This compiles only a small helper test, without CMake/Ninja or an emulator.
It checks actual Unicode/missing/non-directory mappings, a distinct filesystem
when `/dev/shm` exists, values above 4 GiB, `TInt64` overflow, reserved blocks,
read-only behavior and session default-drive parsing. POSIX hosts also exercise
an actual inaccessible root. The result records the tested source hashes.
These checks do not substitute for the separate native `RFs::Volume` probe:
IPC layouts, default-session behavior and write/delete free-space changes
still require execution on the rebuilt emulator.
