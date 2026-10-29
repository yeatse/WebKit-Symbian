# Building the Symbian 2.52.6 port

Use symbian-platform SDK 0.3.0 (Clang 23.1.2), CMake, Ninja, host Clang/LLD, g++, patch, binutils (`readelf`), Ruby with ERB/JSON, Perl, gperf, flex, bison, Python 3.12 or newer with `cryptography` (Ubuntu package `python3-cryptography`), pkg-config and autotools. The build uses C++23, ARMv6 with VFPv2 and softfp, ARM LLInt/Baseline/DFG, and ICU 70.1. [ARM11-JIT.md](ARM11-JIT.md) documents the execution tiers, validation and the `SYMBIAN_JSC_MODE=cloop` fallback.

The sparse checkout must contain `Source/JavaScriptCore`, `Source/WTF`, `Source/WebCore`, `Source/WebKitLegacy`, `Source/bmalloc`, `Source/cmake`, `Source/ThirdParty/ANGLE`, `Source/ThirdParty/unifdef`, `Tools/Scripts` and `Tools/Symbian`.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/symbian-sdk
export SYMBIAN_WEBKIT_WORK=/path/to/work
export JOBS=4
python3 Tools/Symbian/scripts/apply-sdk-patches.py
Tools/Symbian/scripts/build-symbian-native.sh
python3 Tools/Symbian/scripts/rebuild-sdk-runtime.py --platform-source "$SYMBIAN_WEBKIT_WORK/symbian-platform" --install
python3 Tools/Symbian/scripts/rebuild-sdk-rcomp.py --install
python3 Tools/Symbian/scripts/rebuild-sdk-mbedtls-ip.py --install
Tools/Symbian/scripts/build-icu.sh
Tools/Symbian/scripts/build-jsc.sh
Tools/Symbian/scripts/link-e32.sh
Tools/Symbian/scripts/build-deps.sh
Tools/Symbian/scripts/build-webcore.sh
Tools/Symbian/scripts/build-browser.sh
Tools/Symbian/scripts/package-sis.sh
```

`build-webcore.sh` builds `WebCore WebKitLegacy`. `STUB=1` builds only the browser shell and does not validate the engine. Build logs and artifacts stay under `SYMBIAN_WEBKIT_WORK`; do not commit SDKs, firmware, generated objects or private test trust stores.

The runtime step verifies SDK 0.3.0 and its fixed source/archive hashes, rebuilds the ARMv6 allocator with its 32-bit page map, and retains the 64 MiB native budget. It must precede JSC/browser linking. Its original-archive backup and per-object/ABI verification stay in `$SYMBIAN_WEBKIT_WORK/sdk-runtime`; see [SDK patch provenance](../patches/sdk-0.3.0/README.md).

The resource-compiler step fixes a host crash when compiling valid symbolic resource identifiers. It builds pinned source and runs 14 regressions before replacing only the SDK's `rcomp`; its backup, build log and verification records stay in `$SYMBIAN_WEBKIT_WORK/sdk-rcomp`. Run the resource repair before building the browser's native dialogs; their resource structures and identifiers must remain symbolic.

The TLS step adds strict binary IP subject-alternative-name matching and omits IP literals from SNI. It preserves chain, validity and hostname verification, and leaves DNS identity handling intact. Only two ARM objects are replaced after pinned-input, archive/ABI, 35 signed-certificate/ClientHello and 2,069 parser checks; `libmbedcrypto.a` is unchanged. Original archives, build logs and verification records stay in `$SYMBIAN_WEBKIT_WORK/sdk-mbedtls-ip`. See [the SDK patch](../patches/sdk-0.3.0/mbedtls-ip/README.md). CI repeats all SDK repairs after restoring the original SDK cache and before linking target images.

ICU installs `icudt70l.dat` into `C:/resource/icu`; its SIS uses UID `0xE1A5C013`, separate from ICU63. Compiler wrappers resolve configure checks against real SDK import proxies, including libm. Dependencies and patches live under `Tools/Symbian`; changes to upstream sources are scoped to the platform port.

Verify compilation, ELF linking, E32 conversion, SIS installation and guest execution separately. For JSC require a fresh log, expected PASS output, `jsc exiting 0`, and absence of guest panics or access violations; the emulator host's exit code alone is insufficient. Linux ARM QEMU is supplementary and does not exercise Symbian threads, chunks, files or AVKON.

Browser validation proceeds from local DOM/JS/text rendering to WebCrypto vectors, HTTP/WebSocket, HTTPS/WSS and certificate/hostname rejection. The X11 browser runner captures the screen, requests normal emulator window closure and reads logs after the host has flushed its buffered guest file writes. Early exit, nonzero exit and forced shutdown fail the run, even if a PASS marker is present. TLS verification must stay enabled. Any test CA belongs only in the test emulator's trust file and must be restored afterward; never package it.

See [network-tests.md](../tests/network-tests.md) for the HTTP/TLS fixture server, exact report checks and isolated test trust setup.

The recorded ARM11 JIT EKA2L1/X7 browser runs use `yeatse/EKA2L1` branch `webkit-symbian` at `5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a` with patches `0001`, `0005`, `0006`, `0007` and `0008` in `patches/eka2l1` applied to a separate local validation checkout. The two shutdown patches fix stale canvas access and property completion after semaphore destruction; without them a normal emulator close can crash after the guest has finished. The VFS patch makes completed RFile writes immediately visible to filesystem metadata queries, including after a resize reopens the host stream. Without it SQLite can observe a truncated rollback journal and fail to restore its cached page. The CLI installation and panic-dump patches remain validation tools. These patches are saved here for reproduction, not committed or pushed to the EKA repository.

Graphics/media validation adds the subsequent EGL/GLES and MMF patches in numeric order. Rebuild the emulator host, then separately rebuild and deploy its native audio/video patch DLLs using [native-media-patches](../eka2l1/native-media-patches/README.md). The host build otherwise copies old prebuilt guest DLLs. SDK header restoration and converter patches are reproducible inputs; firmware, SDK binaries and private TLS test keys remain outside the repository. Native EGL/ANGLE probes and MMF callback, pixel and PCM checks are separate from browser page validation.

SQLite uses directory locks and DELETE journals because Open C has no POSIX record locking. The SQLite build shim must keep `O_BINARY` zero: this SDK aliases it to write buffering, which makes incomplete journal metadata visible during rollback. See [the guest SQLite regression](../tests/sqlite-probe/README.md) for locking, rollback and persistence checks.

Current ARM11 results are in [ARM11-JIT.md](ARM11-JIT.md). The initial C_LOOP reconstruction is recorded in [Port-2.52.md](Port-2.52.md); historical design and 2.32 results are retained in [Development-2.32.md](Development-2.32.md).
