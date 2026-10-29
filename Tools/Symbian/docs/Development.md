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

Browser validation proceeds from local DOM/JS/text rendering to WebCrypto vectors, HTTP/WebSocket, HTTPS/WSS and certificate/hostname rejection. TLS verification must stay enabled. Any test CA belongs only in the test emulator's trust file and must be restored afterward; never package it.

Current results are in [Port-2.52.md](Port-2.52.md). Historical design and 2.32 results are retained in [Development-2.32.md](Development-2.32.md).
