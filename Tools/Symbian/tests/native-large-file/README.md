# Native large-file descriptor regression

This standalone Symbian executable tests actual OpenC `write`/`read` and native
file-server IPC. It does not link WebKit or change its allocator budget. The
single 37 MiB allocation is reused for every read and write.

Build with `SYMBIAN_SDK_PREFIX`, `SYMBIAN_NATIVE_CONVERTER`, and `PROBE_BUILD` set,
then run `./build.sh`. UID3 is `0xe0000906`, capabilities are zero. The log is
`C:/Data/WebKit/large-file-probe.log`; a strict successful run requires all **37
checks**, `large-file-probe exiting 0`, guest and emulator exit zero, and no panic.
Allow 120 seconds. Only `large-file-owned.bin` and `large-file-ipc.bin` in that
directory belong to this probe.

The full run checks:

- A 37 MiB file written in 1 MiB calls as a control.
- Single writes and single reads at 16 MiB minus one byte, exactly 16 MiB,
  16 MiB plus one byte, 34 MiB and 37 MiB.
- Exact physical length, every byte of the deterministic payload, EOF, and
  SHA-256 of each complete single read. FNV-1a accompanies the chunked reads.
- Zero-length writes and server rejection of negative write lengths or lengths
  larger than the descriptor, without changing the initially empty file.

The malformed IPC cases use the inherited `RSubSessionBase::SendReceive` and the
Belle `EFsFileWrite` function number 35. This intentionally bypasses client-side
`RFile::Write` validation to exercise the server boundary.

For the old emulator only, `--legacy-negative` executes the 37 MiB small-call
control followed by the 34 MiB single write (eight checks). It omits further
boundary and raw IPC cases. The known old descriptor mask keeps only 24 length
bits: 34 MiB becomes 2 MiB. The old file server nevertheless passes the full
requested length to `fwrite`, reading beyond its host string and ignoring a
short write. Thus even the negative mode may crash the host. A mismatch or crash
is a **failed negative control**, never a successful probe. Logs contain only
sizes, hashes and mismatch offsets, never arbitrary file contents; corrupted
files must remain outside the repository. Preserve the original failed result
when rerunning the identical image against the fixed emulator.

This probe validates emulator/file I/O behavior. Browser IndexedDB quota and
Blob payload checks still require the separate `idb-quota` fixture.
