# Building the Symbian 2.52.6 port

Use symbian-platform SDK 0.3.0 (Clang 23.1.2), CMake, Ninja, host Clang/LLD, Ruby with ERB/JSON, Perl, gperf, flex, bison, Python, pkg-config and autotools. The build uses C++23, ARMv6 with VFPv2 and softfp, C_LOOP, and ICU 70.1.

The sparse checkout must contain `Source/JavaScriptCore`, `Source/WTF`, `Source/WebCore`, `Source/WebKitLegacy`, `Source/bmalloc`, `Source/cmake`, `Source/ThirdParty/unifdef`, `Tools/Scripts` and `Tools/Symbian`.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/symbian-sdk
export SYMBIAN_WEBKIT_WORK=/path/to/work
export JOBS=4
Tools/Symbian/scripts/build-symbian-native.sh
Tools/Symbian/scripts/build-icu.sh
Tools/Symbian/scripts/build-jsc.sh
Tools/Symbian/scripts/link-e32.sh
Tools/Symbian/scripts/build-deps.sh
Tools/Symbian/scripts/build-webcore.sh
Tools/Symbian/scripts/build-browser.sh
Tools/Symbian/scripts/package-sis.sh
```

`build-webcore.sh` builds `WebCore WebKitLegacy`. `STUB=1` builds only the browser shell and does not validate the engine. Build logs and artifacts stay under `SYMBIAN_WEBKIT_WORK`; do not commit SDKs, firmware, generated objects or private test trust stores.

ICU installs `icudt70l.dat` into `C:/resource/icu`; its SIS uses UID `0xE1A5C013`, separate from ICU63. Compiler wrappers resolve configure checks against real SDK import proxies, including libm. Dependencies and patches live under `Tools/Symbian`; changes to upstream sources are scoped to the platform port.

Verify compilation, ELF linking, E32 conversion, SIS installation and guest execution separately. For JSC require a fresh log, expected PASS output, `jsc exiting 0`, and absence of guest panics or access violations; the emulator host's exit code alone is insufficient. Linux ARM QEMU is supplementary and does not exercise Symbian threads, chunks, files or AVKON.

Browser validation proceeds from local DOM/JS/text rendering to WebCrypto vectors, HTTP/WebSocket, HTTPS/WSS and certificate/hostname rejection. The X11 browser runner captures the screen, requests normal emulator window closure and reads logs after the host has flushed its buffered guest file writes. Early exit, nonzero exit and forced shutdown fail the run, even if a PASS marker is present. TLS verification must stay enabled. Any test CA belongs only in the test emulator's trust file and must be restored afterward; never package it.

See [network-tests.md](../tests/network-tests.md) for the HTTP/TLS fixture server, exact report checks and isolated test trust setup.

The recorded EKA2L1/X7 browser runs use `yeatse/EKA2L1` branch `webkit-symbian` at `5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a` with the five patches in `patches/eka2l1` applied to a separate local validation checkout. The two shutdown patches fix stale canvas access and property completion after semaphore destruction; without them a normal emulator close can crash after the guest has finished. The VFS patch makes completed RFile writes immediately visible to filesystem metadata queries, including after a resize reopens the host stream. Without it SQLite can observe a truncated rollback journal and fail to restore its cached page. The CLI installation and panic-dump patches remain validation tools. These patches are saved here for reproduction, not committed or pushed to the EKA repository.

Current results are in [Port-2.52.md](Port-2.52.md). Historical design and 2.32 results are retained in [Development-2.32.md](Development-2.32.md).
