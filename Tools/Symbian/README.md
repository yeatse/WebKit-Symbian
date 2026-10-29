# WebKit 2.52.6 for Symbian^3

A development port for Symbian^3, Anna and Belle (ARMv6, VFPv2, softfp), based on WebKitGTK 2.52.6, commit `4fb33923db2f945803df49546f75867980365c08`.

[![Symbian build](https://github.com/yeatse/WebKit-Symbian/actions/workflows/symbian.yml/badge.svg?branch=2.52.6-symbian)](https://github.com/yeatse/WebKit-Symbian/actions/workflows/symbian.yml?query=branch%3A2.52.6-symbian)

Code pushes to `2.52.6-symbian` automatically build and package the port through the [Symbian workflow](https://github.com/yeatse/WebKit-Symbian/actions/workflows/symbian.yml?query=branch%3A2.52.6-symbian), just like the 2.32 branch. Each successful run provides a `sis` artifact containing `symbian-webkit-icu.sis`, `jsc.sis`, `browser.sis` and `SHA256SUMS`; its summary links directly to the download. Install the ICU package first. The `elf` artifact contains the matching browser symbols, and failed builds upload diagnostic logs.

CI packages use version `0.3.<run number>` and use the existing `SIS_CERTIFICATE`/`SIS_KEY` secrets when configured; otherwise they are unsigned. Documentation-only, emulator-helper and QEMU-only pushes retain the 2.32 workflow's skip rules. Avoid `[skip ci]` in commits that should produce packages. The workflow also supports manual runs; only a `symbian-v*` tag publishes a release. CI builds and packages the images; the separate EKA runtime validation is documented below.

This branch reconstructs the previously uncommitted port and adds ARM11 Baseline JIT and DFG. Both EKA2L1/X7 and QEMU ARM1136 pass 12 execution checks in each of LLInt, Baseline and DFG, with matching results, actual tier execution and OSR transitions. Default concurrent DFG compilation and the existing JavaScript/Intl/ArrayBuffer suites also pass. See [ARM11-JIT.md](docs/ARM11-JIT.md) for current build modes and evidence.

The default target is JavaScriptCore's ARM-state LLInt, Baseline JIT and DFG with ICU 70.1 and a single-process WebKitLegacy browser using AVKON, Cairo, FreeType, HarfBuzz, curl and mbedTLS. `SYMBIAN_JSC_MODE=cloop` selects the C_LOOP fallback. Native EGL/GLES2 composition and ANGLE WebGL 1 pass the EKA browser's 10/10 and 9/9 regressions; the repaired text/stripe rendering matches its software reference. See [GRAPHICS.md](docs/GRAPHICS.md) for image hashes, standalone EGL 131/131 and ANGLE 45/45 results, and validation limits. Symbian^3 MMF playback has separate native validation; browser media integration remains pending in [MEDIA.md](docs/MEDIA.md). FTL, WebAssembly, WebGL 2, Web Audio, Service Workers, Shared Workers and Cache API remain disabled. TLS certificate and hostname verification remain enabled.

The initial C_LOOP reconstruction's complete browser, E32, SIS installation, rendering, WebCrypto, networking and SQLite results are preserved in [Port-2.52.md](docs/Port-2.52.md). Those historical artifacts and the 2.32 branch's phone results do not replace validation of a new ARM11 image.

The WebCrypto backend registers SHA-1/256/384/512, HMAC, HKDF, PBKDF2 and AES-CBC/GCM/KW. AES-CFB-8 remains unavailable, matching WebKit 2.52's upstream deprecation. AES-CTR, RSA, EC, Ed25519, X25519 and SHA-224 are unregistered; persistent CryptoKey wrapping is unavailable without a secure system master key.

Before a local build, run `python3 Tools/Symbian/scripts/apply-sdk-patches.py --sdk "$SYMBIAN_SDK_PREFIX"` from the checkout root. This restores the SDK 0.3.0 release's missing Symbian^3 video/surface headers from its own pinned official sources, with source revision and content hash checks; differing installed files cause an error. CI runs it after restoring the SDK cache. Provenance is recorded in [patches/sdk-0.3.0](patches/sdk-0.3.0/README.md).

Also run `python3 Tools/Symbian/scripts/rebuild-sdk-runtime.py --sdk "$SYMBIAN_SDK_PREFIX" --install` before building. This reproducibly repairs the ARMv6 SDK allocator's 32-bit reservation overhead while retaining its 64 MiB budget. It checks the exact SDK/source/archive hashes, preserves every non-mimalloc object and validates the public ABI and ARM attributes. CI runs the same rebuild; [allocator evidence](docs/validation-memory/sdk-mimalloc/README.md) distinguishes this fix from WebKit's separate JSC aligned pool.

See [Development.md](docs/Development.md) for builds and [Handoff-2.52.md](docs/Handoff-2.52.md) for the reconstruction requirements. [Development-2.32.md](docs/Development-2.32.md) and [Handoff.md](docs/Handoff.md) are historical reference material.
