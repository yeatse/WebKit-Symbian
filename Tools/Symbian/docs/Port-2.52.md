# WebKit 2.52.6 Symbian reconstruction

Updated: 2026-10-29 (Asia/Singapore).

## Provenance

- Base: WebKitGTK `webkitgtk-2.52.6`, `4fb33923db2f945803df49546f75867980365c08`.
- Reference: `2.32.4-symbian`, `165777485aca391b73583d12d45edb230d3c4216`; reference upstream `a81ec020d36acabbe870529ac29582c954c91852`.
- The reference now includes later crypto/media commits after the supplied handoff's `4dca1b91`. Media stays disabled on this branch.
- Emulator: `yeatse/EKA2L1`, `webkit-symbian`, `5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a`. CLI firmware installation and panic diagnostics are local validation changes only. Browser validation also uses two local shutdown fixes: message-window animations observe canvas destruction, and property subscriptions do not signal already-destroyed request semaphores during kernel wipeout. All four validation patches are preserved under `patches/eka2l1` in this WebKit repository for reproduction; none is committed or pushed to the EKA repository. The final browser-validation emulator SHA256 is `94bcad8b14b1ac7d6b78ddaa33f717cfcc432ca008d556af6631cbe21da3eb7f`.
- SDK: symbian-platform 0.3.0, Clang 23.1.2. Release archive SHA256 `9be830884a5fd1b9315ea1c11571810f7a50fa673851fa392bb397c6759fec3d`.

## Current checkpoint

The source port has been reconstructed and saved to `2.52.6-symbian` in staged commits. ICU 70.1 and all third-party target dependencies have compiled. JSC has compiled, linked and converted to E32: `jsc.exe` is 14,305,772 bytes, SHA256 `77a73defd8f84e260455d9416e6b191f803f7a128912d43d5baf297780aa3b02`. ICU data SHA256 is `5fe6ac8866742956f7fac9bcee571b6450cb56808445c19e544c08bdea66755f`. All four fresh EKA2L1/X7 runs pass with the expected PASS marker, `jsc exiting 0`, host exit 0 and no guest panic/access violation:

- basic: `jsc-20261029T091922Z-483yuvby`
- language: `jsc-20261029T091940Z-1qf4zvv_`
- Intl: `jsc-20261029T091942Z-fp4lf4fw`
- RAB/GSAB/GC: `jsc-20261029T091943Z-a6ugcdvw`, with `--useSharedArrayBuffer=true`

Run directories are under the local work directory's `runs/`. RAB covers page growth/data preservation, shrinking/regrowth zeroing and bounds; GSAB covers growth and rejected shrinking. Guest OOM fault injection and physical-device behavior have not been tested.

The ICU and JSC SIS packages also install successfully in X7: each reports `Installation done!` with its matching UID and package registry; the installed EXE/data and all four test scripts match the build bytes. JSC installs to `E:/sys/bin/jsc.exe`, ICU data to `C:/resource/icu/icudt70l.dat`. The build packages are 5,189,128 bytes (`jsc.sis`) and 4,744,200 bytes (`symbian-webkit-icu.sis`). The GUI is terminated after installation evidence is captured, so its termination status is not used as the install verdict.

WTF, PAL, JSC, WebCore and WebKitLegacy have all compiled and archived for ARM. The complete AVKON browser has linked and converted to E32:

- `browser.elf`: 91,861,652 bytes; `.ARM.exidx`: 731,808 bytes (91,476 records), successfully accepted by the patched converter.
- `browser.exe`: 49,121,832 bytes; SHA256 `282d7dbfa7566cf3fda437b0e61c55663722987352594ce7b7c3e15dd3a1a34f`.
- `browser.sis`: 18,355,252 bytes; SHA256 `8eebe8516eb9006d849a4ce10be160c46184288cecd8cde7d5b7f1dc4f5d2bed`.

The browser SIS installs successfully in X7 with UID `0xE1A5C012`, the installation-complete marker and package registry. The installed EXE, registration/localization resources, local HTML and release CA bundle match the package inputs. The local DOM/modern JavaScript/Chinese rendering page passes in `browser-20261029T102646Z-_97vb6hr`, with a fresh console marker, screenshot, normal host shutdown and no guest panic/access violation. The form controls render; interactive input has not been separately tested.

The E32 converter's 47 host regression tests pass, including a valid exception index larger than 64 KiB, malformed table/descriptor rejection and strict single-branch ARM import thunks. Clean sequential patch application, reverse checks and repeated cached builds pass. Converter SHA256: `6639323282ca92f09c823a9ea7178b318c2198cfdf27c5e0bb9e50cef6d5f27d`.

Host fault injection compiles the actual `SymbianSystem.cpp` against fake kernel interfaces and passes reservation/commit rollback, data preservation, decommit/zeroing, slot limit/reuse and entropy failure/short-fill checks. Filesystem host regressions also pass five groups covering path handling, aliases/no-overwrite, non-regular files, short writes/EINTR, failed-copy cleanup, cross-device fallback, temporary directories and permission errors. Six fake-emulator JSC runner regressions and twelve browser-network runner host regressions pass. These validate algorithms and harness behavior; they do not substitute for guest kernel or JSC execution.

Browser fixture pages cover HTTP/WebSocket and TLS. The first complete WebCrypto guest run passed 18 of 19 groups; AES-CFB-8 was rejected by WebKit 2.52's intentional API deprecation. This is preserved: CFB is removed from the platform registry and its fixture checks rejection. The historical CFB implementation/registration claim is not evidence of usable browser API support, and no Symbian CFB vector pass is claimed. The initial browser image passes all six TLS groups in `tls-validation-282d7dbf/result.json`: trusted HTTPS/WSS and independent wrong-hostname/untrusted-root rejection for each. The test CA is restored to its exact original bytes. Initial HTTP results are 20/23: cookie persistence and cross-port redirect handling require correction. Final-image WebCrypto and network outcomes are pending.

Verified environment preparation: SDK compiler, linker and stock E32 converter built the SDK hello-time example. This does not validate WebKit. SDK entropy source uses the checked kernel secure RNG path with a 1024-byte request limit; emulator behavior cannot establish physical-device entropy quality.

## Required validation

- WebCrypto standard vectors and error inputs.
- HTTP, WebSocket, HTTPS, WSS, untrusted certificates and hostname mismatch.

## Boundaries

C_LOOP only; no JIT/DFG/FTL/Wasm/sampling profiler. No GPU compositor, WebGL or multimedia backend. Service Worker, Shared Worker and Cache API remain off. Register only implemented WebCrypto algorithms; reject unsupported algorithms and persistent key wrapping without a secure system master key. RChunk has no general mprotect/read-only equivalent; decommit destroys contents. Its metadata table supports 256 simultaneous reservations (ordinary malloc is separate), and initial resizable ArrayBuffer maximum storage can still be temporarily fully committed. Filesystem hard-link and capacity operations must report unsupported behavior honestly. Emulator results do not establish physical-phone behavior.

The supplied reconstruction record is preserved in [Handoff-2.52.md](Handoff-2.52.md). Its old validation results are historical, not evidence for this checkout.
