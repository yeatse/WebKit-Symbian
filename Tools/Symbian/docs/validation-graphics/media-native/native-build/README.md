# Native-only MMF DLL build evidence

The source wrapper is [native-media-patches](../../../../eka2l1/native-media-patches/README.md).
These records describe the candidate package produced at 2026-10-29 15:19:14 UTC.
No DLL, SDK, ROM or signing key is stored here. `build-inputs.json` records the
external package location, SDK/converter identity, wrapper hashes and native
source hashes; `SHA256SUMS` identifies the external DLL and original map files.

| Image | Bytes | Frozen exports | EHABI index |
| --- | ---: | --- | ---: |
| `mediaclientaudio_general.dll` | 72,620 | 118 present, including 18 read-only objects | 1,376 bytes |
| `mediaclientvideo_v100.dll` | 52,556 | 165 ordinals: 83 present, 82 absent; 4 read-only objects | 960 bytes |

Both images use the native freestanding DLL entry, retain their leave/unwind
descriptor and have zero writable data/BSS and zero C++ lifetime arrays. Native
deallocation calls `User::Free`, matching `CBase`'s `User::AllocZL`; it no longer
pulls the portable SDK allocator, TLS or stdio into an early ROM-routed patch.
Audio imports only `drtaeabi`, `efsrv`, `euser` and `mediaclient`; video also
imports `ws32`. The video link extracts 14 explicitly allowed stateless compiler
arithmetic helpers. Neither image imports libc, pthread or itself.

The previous portable-runtime DLLs produced real unresolved Open C/Pthread
ordinal warnings during EKA's pre-process ROM attachment, and paired native
allocation with the wrong delete implementation. They are superseded, not
treated as harmless because some earlier API tests passed.

`native-contract.json` is the wrapper's automatic build check.
`native-media-dll-verification.json` compares every present frozen export with
the actual linked ELF and records read-only object ordinals and EHABI sizes.
Each `*-real-elf-tests.json` records a successful conversion of the final ELF
followed by rejection of 20 independently corrupted metadata variants (21/21).
`native-contract-regression.json` preserves the separate old/new contract check;
its private A/B output hashes differ from this canonical package.

The clean-host WAV, HTTPS WAV, MP3 and video runs in [audio](../audio/README.md)
identify these exact DLL hashes, exit with guest/host zero, and pass independent
PCM analysis. `guest-contract.json` records their identities and host log hashes;
the compressed original logs are retained beside the PCM evidence. Their logs
have no invalid-ordinal warning; Close/Delete completes
normally. Their host `ff218e2e…` removes temporary MMF diagnostics. Earlier
trace-host candidates remain separately identified. Native API/PCM validation
does not establish browser integration.
