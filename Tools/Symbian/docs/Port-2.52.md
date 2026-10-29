# WebKit 2.52.6 Symbian reconstruction

Validated: 2026-10-29 (Asia/Singapore). The reconstructed source, build tools, tests and this record are saved on `2.52.6-symbian`. The final engine revision is `93fca3f63fe5ad74424d8cfaf09388f182924821`; later commits preserve tests and evidence.

## Provenance

- Base: WebKitGTK `webkitgtk-2.52.6`, `4fb33923db2f945803df49546f75867980365c08`.
- Reference: `2.32.4-symbian`, `165777485aca391b73583d12d45edb230d3c4216`; reference upstream `a81ec020d36acabbe870529ac29582c954c91852`. Media remains disabled despite later media work on the reference branch.
- SDK: symbian-platform 0.3.0, Clang 23.1.2; release archive SHA256 `9be830884a5fd1b9315ea1c11571810f7a50fa673851fa392bb397c6759fec3d`.
- Emulator: `yeatse/EKA2L1`, `webkit-symbian`, `5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a`, with X7 / RM-707 firmware. Five patches in `Tools/Symbian/patches/eka2l1` reproduce the local validation checkout. No changes were committed or pushed to the EKA repository.

The EKA patches provide CLI firmware installation, panic diagnostics, two shutdown lifetime fixes, and coherent host file-write visibility. The shutdown fixes prevent stale canvas access and property notifications after request semaphore destruction. The VFS patch removes host stdio buffering that can hide completed guest writes from metadata queries. Final browser-validation emulator SHA256: `c6c98382236bb8e83f31ae545c3e11edb126dd5d61f87e5b4ff07a22cd92fb1c`.

## Build and installation

ICU 70.1, all target dependencies, WTF, PAL, JSC, WebCore, WebKitLegacy, the AVKON shell and resources compile. Both complete executables link and convert to E32. The browser ELF is 91,852,560 bytes; its `.ARM.exidx` is 731,720 bytes (91,465 entries), accepted without the old arbitrary 64-KiB limit.

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| `jsc.exe` | 14,305,772 | `77a73defd8f84e260455d9416e6b191f803f7a128912d43d5baf297780aa3b02` |
| `browser.exe` | 49,115,500 | `b9eff228efeda9fa831070440311ab4ff8d3af6c070cc3f0f70f9933712858d4` |
| `jsc.sis` | 5,189,128 | `e0b307c3af54e1cb03f761e083c6daf61337ac4cf52a923c536fba569cc74d59` |
| `browser.sis` | 18,354,312 | `c8bb1c406a1fc8b1817dd1dd900fa8853dbf6a68331e35a98eb12429465a7452` |
| `symbian-webkit-icu.sis` | 4,744,200 | `9bca263c767e2095b5835340a2bc5a7a953a95e9a9a2082e5bd7c60c46b88bb8` |

All three SIS packages were actually installed in X7 and verified through installation markers, package registry and byte comparisons. The final browser SIS upgraded the earlier image on `E:/sys/bin/browser.exe`; its EXE, registration/localization resources, HTML and release CA all match the package inputs. The runtime copy on C and the installed copy on E have the final hash above. ICU data installs to `C:/resource/icu/icudt70l.dat`, separately owned by UID `0xE1A5C013`. Install the ICU SIS before JSC or the browser. Packages are unsigned development builds.

## Actual guest results

Four JSC suites pass with fresh PASS output, `jsc exiting 0`, host exit 0 and no guest panic/access violation: basic, modern language, Intl, and RAB/GSAB/GC. The last uses `--useSharedArrayBuffer=true` and checks growth, preserved data, shrinking/regrowth zeroing and bounds. Logs are preserved in [validation-2.52](validation-2.52).

The final browser image passes all of the following with fresh logs, normal host exit and no guest panic/access violation:

| Suite | Result | Evidence |
| --- | --- | --- |
| Local DOM, modern JavaScript, Chinese and CSS rendering | PASS | [log](validation-2.52/local.log), [screenshot](validation-2.52/local.png) |
| WebCrypto | 19/19 | [log](validation-2.52/webcrypto.log) |
| HTTP, XHR, redirects, cancellation, cookies and WebSocket | 23/23 | [report](validation-2.52/network-report.json) |
| HTTPS/WSS success, wrong hostname and untrusted root | 6/6 | [report](validation-2.52/tls-report.json) |

The local form controls render; interactive input has not been separately tested. WebCrypto covers 12 groups of supported operations/error inputs and seven rejection groups. AES-CFB-8 is deliberately rejected, matching WebKit 2.52's API deprecation; the historical claim of an implemented/registered backend did not establish an exposed API. No Symbian CFB vector pass is claimed. AES-KW `wrapKey`/`unwrapKey` works; platform master-key wrapping for persistent CryptoKeys remains unsupported.

HTTP checks include sync/async 401 completion, cookie path/deletion/recalculation, cross-port authorization removal with legal host cookies, text/binary WebSocket, and local-file/forbidden-port redirect rejection. Redirect continuation preserves the Origin recomputed by the client. TLS tests retain peer and hostname verification. Each negative WSS case independently requires an error and no open event; guest logs report certificate-verification errors. The host also verifies all three endpoint identities and exact rejection reasons. Test trust was restored byte-for-byte and never packaged. The release CA SHA256 remains `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`.

## Storage and tool regressions

Open C rejects SQLite's default POSIX record locks. Symbian therefore selects SQLite's `unix-dotfile` directory locks and explicitly verifies DELETE journal mode. The dependency shim also clears SQLite's `O_BINARY`: this SDK aliases it to `O_BUFFERED`, which otherwise hides the journal's final four-byte checksum and breaks immediate rollback readback. Binary I/O is already the platform default.

The [standalone guest regression](../tests/sqlite-probe/README.md), linked to the exact browser SQLite archive, passes lock exclusion, writes after lock release, natural rollback on the same connection, and close/reopen persistence. No extra journal sync is injected. Both journals have the full 4,616 bytes before rollback. The final run is `sqlite-probe-final-kmynPhy9`; its [log](validation-2.52/sqlite-probe.log) and archive hash are preserved.

The E32 converter passes 47 host regressions, including large valid exception tables, malformed descriptor/table rejection and strict ARM import thunks. Sequential clean patch application, reverse checks and repeated cached builds pass. Native host fault injection passes three memory/entropy groups and five filesystem groups; these compile the actual platform implementations with fake kernel interfaces or host filesystem shims. Runner regressions pass six JSC, five browser and twelve network cases. Eight local EKA VFS checks cover metadata visibility, cross-handle reads and resize/reopen behavior. These host checks do not substitute for guest or physical-device validation.

[results.json](validation-2.52/results.json) records artifact hashes, run identities, installation manifests and evidence hashes. Paths inside raw manifests refer to the original validation workspace. [Development.md](Development.md) describes rebuilding; [Handoff-2.52.md](Handoff-2.52.md) preserves the supplied historical reconstruction record.

## Boundaries

C_LOOP only: no JIT/DFG/FTL/Wasm/sampling profiler. No GPU compositor, WebGL or multimedia backend. Service Worker, Shared Worker and Cache API stay off. Only implemented, non-deprecated WebCrypto APIs are registered; AES-CTR, RSA, EC, Ed25519, X25519 and SHA-224 remain unavailable.

RChunk has no general mprotect/read-only equivalent; decommit destroys contents. Its metadata supports 256 simultaneous reservations, and initial resizable ArrayBuffer maximum storage can still be temporarily fully committed. Filesystem hard links and capacity queries report unsupported behavior. SQLite uses exclusive directory locks; abnormal termination may leave a lock directory, and crash/power-loss recovery and stale-lock recovery have not been validated. Open C directory syncing is unavailable.

Physical-phone behavior, guest OOM fault injection, complete WebCrypto conformance and QEMU JSC execution are not validated. All results here are new checks of this reconstruction, not inherited 2.32 results. No release tag or pull request was created.
