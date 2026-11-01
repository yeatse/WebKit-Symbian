# EKA descriptor and file IPC regression

`0024-Preserve-descriptor-lengths-and-bound-file-IPC-transfers.patch` fixes an
emulator defect exposed by a single 34 MiB IndexedDB Blob sidecar write. Symbian
uses the low **28** bits of a descriptor's first word for length and the high
four for type (`e32des8.h`, `e32des16.h`, `e32cmn.inl`). The emulator used only 24
length bits. Its file server copied 2 MiB from the 34 MiB descriptor, then passed
the independent 34 MiB request count to `fwrite`, reading beyond that copy and
reporting success even when the host wrote fewer bytes.

The fix covers the getter, type setter, length setter and packed-length helper.
Maximum length is a separate word; it is not a packed type/length field. File IPC
validates headers, descriptor type/capacity, request length and position, every
payload page's mapping and permission, and 32-bit guest address overflow. It
copies through a bounded 64 KiB staging buffer. Virtually adjacent guest pages
may have unrelated host backing, including descriptor headers crossing pages.
This changes the emulator's IPC implementation, not WebKit's Blob producer or
quota threshold. One guest request still writes/reads the entire descriptor.

A negative length/position or write length exceeding a valid source descriptor is
`KErrArgument`. A read length exceeding output capacity is `KErrOverflow`, as
specified by the SDK `f32file64.inl` documentation for `RFile::Read`. Bad metadata,
inaccessible mappings and read-only output buffers are `KErrBadDescriptor`. Native short writes return `KErrWrite`, never success. Host seek/size failures and unexpected short reads
before EOF return `KErrGeneral`. A real EOF read succeeds with a shortened or
empty descriptor. Partial writes are not rolled back; the error is returned so
the caller can discard the failed operation. Zero-length writes do not extend
the file or dereference a null payload. A positioned write beyond EOF starts at
EOF, matching the same official SDK header (lines 430 and 505); the former
unsafe gap-fill operation is removed.

Run against the EKA tree after applying the patch:

```sh
python3 Tools/Symbian/tests/eka-file-io/run-host.py \
  --eka /path/to/EKA2L1 --work /tmp/eka-file-io --sanitize
```

The runner extracts the **actual** descriptor definitions/methods and file IPC
functions into a temporary translation unit. The only substituted parts are
guest mappings, IPC scaffolding and the VFS boundary. Results record full source
hashes. AddressSanitizer and UndefinedBehaviorSanitizer instrument the executable.
The 34 MiB case uses a real private `tmpfile`, a pattern checked at every byte,
and one invocation of each actual IPC handler. Tests also cover:

- 16 MiB − 1, 16 MiB, 16 MiB + 1, 34 MiB and 28-bit maximum lengths for all types;
  type preservation, shrinking and HBufC/TPtr length propagation.
- Actual full-file writes/reads, valid prefixes, source/capacity mismatch, negative
  or missing arguments, readonly files, invalid descriptors and offsets.
- EOF-clamped positioned writes, failed native writes, failed seek/size, and read
  EOF, unexpected short reads and concurrent truncation.
- Noncontiguous host backing, cross-page descriptor headers, missing second
  pages, readonly output pages and overflowing guest address ranges.

The same test against the saved pre-patch source fails at the first 16 MiB
length boundary. The host test does not claim native Symbian IPC acceptance or
IndexedDB quota correctness. `../native-large-file/` is the independent native
RFile probe; its guest evidence and the original Blob failure remain separate.
