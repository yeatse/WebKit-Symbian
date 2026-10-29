# WebKit 2.52.6 for Symbian^3

A development port for Symbian^3, Anna and Belle (ARMv6, VFPv2, softfp), based on WebKitGTK 2.52.6, commit `4fb33923db2f945803df49546f75867980365c08`.

[![Symbian build](https://github.com/yeatse/WebKit-Symbian/actions/workflows/symbian.yml/badge.svg?branch=2.52.6-symbian)](https://github.com/yeatse/WebKit-Symbian/actions/workflows/symbian.yml?query=branch%3A2.52.6-symbian)

Code pushes to `2.52.6-symbian` automatically build and package the port through the [Symbian workflow](https://github.com/yeatse/WebKit-Symbian/actions/workflows/symbian.yml?query=branch%3A2.52.6-symbian), just like the 2.32 branch. Each successful run provides a `sis` artifact containing `symbian-webkit-icu.sis`, `jsc.sis`, `browser.sis` and `SHA256SUMS`; its summary links directly to the download. Install the ICU package first. The `elf` artifact contains the matching browser symbols, and failed builds upload diagnostic logs.

CI packages use version `0.3.<run number>` and use the existing `SIS_CERTIFICATE`/`SIS_KEY` secrets when configured; otherwise they are unsigned. Documentation-only, emulator-helper and QEMU-only pushes retain the 2.32 workflow's skip rules. Avoid `[skip ci]` in commits that should produce packages. The workflow also supports manual runs; only a `symbian-v*` tag publishes a release. CI builds and packages the images; the separate EKA runtime validation is documented below.

This branch reconstructs the previously uncommitted port. The full JSC and browser builds, E32 conversion and SIS installation pass. Fresh EKA2L1/X7 runs pass four JSC suites, local rendering, WebCrypto 19/19, HTTP/WebSocket 23/23, TLS 6/6, and SQLite locking/rollback/persistence. The 2.32 branch's screenshots and phone results do not validate this version. Current evidence and remaining work are recorded in [Port-2.52.md](docs/Port-2.52.md).

The target is JavaScriptCore's C_LOOP interpreter with ICU 70.1 and a single-process WebKitLegacy browser using AVKON, Cairo, FreeType, HarfBuzz, curl and mbedTLS. JIT, DFG, FTL, WebAssembly, hardware compositing, WebGL, media, Service Workers, Shared Workers and Cache API are disabled. TLS certificate and hostname verification remain enabled.

The WebCrypto backend registers SHA-1/256/384/512, HMAC, HKDF, PBKDF2 and AES-CBC/GCM/KW. AES-CFB-8 remains unavailable, matching WebKit 2.52's upstream deprecation. AES-CTR, RSA, EC, Ed25519, X25519 and SHA-224 are unregistered; persistent CryptoKey wrapping is unavailable without a secure system master key.

See [Development.md](docs/Development.md) for builds and [Handoff-2.52.md](docs/Handoff-2.52.md) for the reconstruction requirements. [Development-2.32.md](docs/Development-2.32.md) and [Handoff.md](docs/Handoff.md) are historical reference material.
