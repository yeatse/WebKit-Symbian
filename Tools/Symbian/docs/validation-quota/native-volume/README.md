# Native mapped-volume A/B verification

The same native executable passed all ten checks after EKA patch 0018. Before
the patch, four checks failed. Both runs exited the host normally without a
guest panic or access violation. The old guest deliberately returned 1 for its
failed assertions; the new guest returned 0.

| Recorded run | Host SHA-256 prefix | Result | Host / guest exit |
| --- | --- | --- | --- |
| `20261029T185946Z-2kvutst8` | `12c275e7f7b3` | **FAIL: 6/10** | 0 / 1 |
| `20261029T190325Z-07gd9jjv` | `5f8704eb1c24` | **PASS: 10/10** | 0 / 0 |

Both used probe SHA-256
`386b483f26f20af074917f256e5213f58e0788f41ce553ddf5f24c85c416f9ec`.
The original result files record the complete host and executable identities,
and the native DLL/map identities independently for each run.

## What changed

The previous emulator returned 1,073,741,824 bytes for both capacity and writable
space. Its four failed checks were read-only ROM writable space, rejection of
an unmounted drive, selection of the RFs session's default drive, and the
space decrease after a flushed write. Those failures remain unchanged in
`old-host/`. Its three “mounted volume and 64-bit native capacity” successes
only compared the bridge with the native result and checked the range; those
old 1 GiB values do not establish a capacity larger than 32 bits.

With patch 0018, C, E and Z each reported **33,770,192,896 bytes**, matching the
capacity of their mapped host filesystem. C and E initially reported
6,952,808,448 available bytes; Z correctly reported zero writable bytes.
This describes an emulator mapping, not a phone's physical ROM capacity.

The probe wrote and flushed a unique **2,097,152-byte** file. C's available
space fell from 6,952,808,448 to 6,950,703,104 bytes: **2,105,344 bytes**.
The extra 8,192 bytes may include other host allocation such as emulator logging;
this is not an assertion of exact file-only allocation. The runtime owner ran
the check without concurrent builds or downloads. The probe then successfully
deleted its own uniquely named file.

`host-filesystem-comparison.json` preserves the independent host `statvfs`
samples, including both user-available bytes and total free bytes, which include
space reserved from ordinary users. All three mapped roots had the same 33,770,192,896-byte
capacity and 4,096-byte fragment size. These host samples surround the run,
not the exact two guest write measurements; intervening logs and cleanup mean
the available-space values need not be byte-for-byte equal. The host backing
filesystem has available space for Z, while the guest read-only policy returns
zero writable space.

## Identity and preservation

`identities.json` records the actual ELF and E32 hashes, source snapshots, SDK
inputs, header fields, and patch hash. The probe is ARMv6 / Thumb-1 with
`-mfpu=none -mfloat-abi=soft`, UID3 `0xe00008f7`, zero capabilities, and E32
floating-point type 0. `build/elf-attributes.txt` contains the actual ELF
attributes; this small probe does not claim to require VFPv2. The recorded link
uses SDK 0.3.0's `lib/armv6/libsymbian_guest_runtime.a` (`726c69c4…`).
Original compile commands and build metadata are retained, along with the
current source snapshots. SDK/source hashes inspected when archiving
are distinguished from identities captured by the runtime runner; this
archive did not rebuild the executable. The build owner also confirmed that
`FileSystemSymbianNative.cpp` and its header have not changed since the recorded
probe was built; that confirmation is identified separately from runner output.

`host-0018-identities.json` records the host's two patched source hashes and
the final native MMF assets restored after the host rebuild. The two guest
result files record identical DLL/map hashes. These are deployment identity
checks; the volume probe does not exercise MMF playback.

All files from both original run directories are retained. Raw logs are
losslessly gzip-compressed without editing, filtering or truncation;
`raw-index.json` records their original lengths and SHA-256 values. The
`previous-volume-probe.log.gz` in `new-host/` is explicitly the retained stale
log from the earlier failed run, not the new run's result. Empty audio artifacts
are retained too; no audio or screenshot assertion was made. Original JSON
metadata is copied byte-for-byte. Absolute paths in those original records
describe the original workspace and are not required by the verifier.

Run `python3 verify-evidence.py` from any directory to check the archive hashes,
decompress every original artifact, and verify the recorded A/B counts,
identities, capacities and write delta. To reproduce on a configured SDK and
emulator, use the [native probe instructions](../../../tests/native-volume/README.md)
and the frozen source under `source-snapshot/`; runtime execution must retain
the emulator's exclusive lock and the isolated write interval.

## Scope

This is a direct native `RFs::Volume` / `volumeInformation` bridge test. It does
not establish IndexedDB quota enforcement, low-disk transaction behavior, or
browser filesystem integration. The separate host tests for
`tryListDirectory` / `directorySizeIfExists` are not guest browser results. The
[host helper evidence](../host-volume/README.md) remains a separate record of
its earlier syntax, permission and patch-order checks.

No executable, SDK, firmware, SIS package, or previous user registry is stored
in this archive.
