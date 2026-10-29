# WebKit 2.52.6 Symbian reconstruction

Updated: 2026-10-29 (Asia/Singapore).

## Provenance

- Base: WebKitGTK `webkitgtk-2.52.6`, `4fb33923db2f945803df49546f75867980365c08`.
- Reference: `2.32.4-symbian`, `165777485aca391b73583d12d45edb230d3c4216`; reference upstream `a81ec020d36acabbe870529ac29582c954c91852`.
- The reference now includes later crypto/media commits after the supplied handoff's `4dca1b91`. Media stays disabled on this branch.
- Emulator: `yeatse/EKA2L1`, `webkit-symbian`, `5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a`. CLI firmware installation and panic diagnostics are local validation changes only.
- SDK: symbian-platform 0.3.0, Clang 23.1.2. Release archive SHA256 `9be830884a5fd1b9315ea1c11571810f7a50fa673851fa392bb397c6759fec3d`.

## Current checkpoint

The source port is being reconstructed. Build tools have been migrated to ICU70/C_LOOP, the reference tooling is retained, and platform API migration is in progress. No new JSC or browser runtime pass is claimed at this checkpoint.

Verified environment preparation: SDK compiler, linker and stock E32 converter built the SDK hello-time example. This does not validate WebKit. SDK entropy source uses the checked kernel secure RNG path with a 1024-byte request limit; emulator behavior cannot establish physical-device entropy quality.

## Required validation

- WTF/JSC cross compilation, final ELF and E32 conversion.
- Fresh EKA JSC basic/language/Intl/RAB/GSAB tests.
- WebCore/PAL/Legacy and browser compilation, ELF, E32 and SIS.
- Local browser DOM/JS/Chinese rendering and input.
- WebCrypto standard vectors and error inputs.
- HTTP, WebSocket, HTTPS, WSS, untrusted certificates and hostname mismatch.
- Converter malformed/large exception-index fixtures and idempotent SDK patches.

## Boundaries

C_LOOP only; no JIT/DFG/FTL/Wasm/sampling profiler. No GPU compositor, WebGL or multimedia backend. Service Worker, Shared Worker and Cache API remain off. Register only implemented WebCrypto algorithms; reject unsupported algorithms and persistent key wrapping without a secure system master key. RChunk has no general mprotect/read-only equivalent; decommit destroys contents. Filesystem hard-link and capacity operations must report unsupported behavior honestly. Emulator results do not establish physical-phone behavior.

The supplied reconstruction record is preserved in [Handoff-2.52.md](Handoff-2.52.md). Its old validation results are historical, not evidence for this checkout.
