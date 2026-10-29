# WebKit 2.52.6 Symbian reconstruction

Updated: 2026-10-29 (Asia/Singapore).

## Provenance

- Base: WebKitGTK `webkitgtk-2.52.6`, `4fb33923db2f945803df49546f75867980365c08`.
- Reference: `2.32.4-symbian`, `165777485aca391b73583d12d45edb230d3c4216`; reference upstream `a81ec020d36acabbe870529ac29582c954c91852`.
- The reference now includes later crypto/media commits after the supplied handoff's `4dca1b91`. Media stays disabled on this branch.
- Emulator: `yeatse/EKA2L1`, `webkit-symbian`, `5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a`. CLI firmware installation and panic diagnostics are local validation changes only.
- SDK: symbian-platform 0.3.0, Clang 23.1.2. Release archive SHA256 `9be830884a5fd1b9315ea1c11571810f7a50fa673851fa392bb397c6759fec3d`.

## Current checkpoint

The source port is being reconstructed. ICU 70.1 and all third-party target dependencies have compiled. JSC has compiled, linked and converted to E32: `jsc.exe` is 14,305,772 bytes, SHA256 `77a73defd8f84e260455d9416e6b191f803f7a128912d43d5baf297780aa3b02`. ICU data SHA256 is `5fe6ac8866742956f7fac9bcee571b6450cb56808445c19e544c08bdea66755f`. All four fresh EKA2L1/X7 runs pass with the expected PASS marker, `jsc exiting 0`, host exit 0 and no guest panic/access violation:

- basic: `jsc-20261029T091922Z-483yuvby`
- language: `jsc-20261029T091940Z-1qf4zvv_`
- Intl: `jsc-20261029T091942Z-fp4lf4fw`
- RAB/GSAB/GC: `jsc-20261029T091943Z-a6ugcdvw`, with `--useSharedArrayBuffer=true`

Run directories are under the local work directory's `runs/`. RAB covers page growth/data preservation, shrinking/regrowth zeroing and bounds; GSAB covers growth and rejected shrinking. Guest OOM fault injection and physical-device behavior have not been tested.

WebCore configuration succeeds and PAL has compiled and archived. The AVKON shell's 11 ARM objects have compiled; 21 WebCore/PAL platform and crypto translation units and 6 Legacy embedding translation units pass actual ARM/C++23 syntax checks. Full WebCore/Legacy archives, final browser ELF/E32, SIS and guest execution are still pending.

The E32 converter's 47 host regression tests pass, including a valid exception index larger than 64 KiB, malformed table/descriptor rejection and strict single-branch ARM import thunks. Clean sequential patch application, reverse checks and repeated cached builds pass. Converter SHA256: `6639323282ca92f09c823a9ea7178b318c2198cfdf27c5e0bb9e50cef6d5f27d`. This does not establish full-browser E32 conversion.

Host fault injection compiles the actual `SymbianSystem.cpp` against fake kernel interfaces and passes reservation/commit rollback, data preservation, decommit/zeroing, slot limit/reuse and entropy failure/short-fill checks. Six fake-emulator runner regressions pass. These validate algorithms and harness behavior; they do not substitute for guest kernel or JSC execution.

Browser fixture pages cover HTTP/WebSocket and TLS. WebCrypto vectors have supplementary host checks, but all browser fixture outcomes still need to be established in the Symbian engine.

Verified environment preparation: SDK compiler, linker and stock E32 converter built the SDK hello-time example. This does not validate WebKit. SDK entropy source uses the checked kernel secure RNG path with a 1024-byte request limit; emulator behavior cannot establish physical-device entropy quality.

## Required validation

- WebCore/PAL/Legacy and browser compilation, ELF, E32 and SIS.
- Local browser DOM/JS/Chinese rendering and input.
- WebCrypto standard vectors and error inputs.
- HTTP, WebSocket, HTTPS, WSS, untrusted certificates and hostname mismatch.

## Boundaries

C_LOOP only; no JIT/DFG/FTL/Wasm/sampling profiler. No GPU compositor, WebGL or multimedia backend. Service Worker, Shared Worker and Cache API remain off. Register only implemented WebCrypto algorithms; reject unsupported algorithms and persistent key wrapping without a secure system master key. RChunk has no general mprotect/read-only equivalent; decommit destroys contents. Filesystem hard-link and capacity operations must report unsupported behavior honestly. Emulator results do not establish physical-phone behavior.

The supplied reconstruction record is preserved in [Handoff-2.52.md](Handoff-2.52.md). Its old validation results are historical, not evidence for this checkout.
