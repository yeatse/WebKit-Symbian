# WebKit 2.52.6 for Symbian^3

A development port for Symbian^3, Anna and Belle (ARMv6, VFPv2, softfp), based on WebKitGTK 2.52.6, commit `4fb33923db2f945803df49546f75867980365c08`.

This branch reconstructs the previously uncommitted port. Compilation and runtime results must be established again; the 2.32 branch's screenshots and phone results do not validate this version. Current evidence and remaining work are recorded in [Port-2.52.md](docs/Port-2.52.md).

The target is JavaScriptCore's C_LOOP interpreter with ICU 70.1 and a single-process WebKitLegacy browser using AVKON, Cairo, FreeType, HarfBuzz, curl and mbedTLS. JIT, DFG, FTL, WebAssembly, hardware compositing, WebGL, media, Service Workers, Shared Workers and Cache API are disabled. TLS certificate and hostname verification remain enabled.

See [Development.md](docs/Development.md) for builds and [Handoff-2.52.md](docs/Handoff-2.52.md) for the reconstruction requirements. [Development-2.32.md](docs/Development-2.32.md) and [Handoff.md](docs/Handoff.md) are historical reference material.
