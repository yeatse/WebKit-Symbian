# Symbian SDK integration

These additions provide the SDK interfaces and runtime support used by the
browser. Build setup is described in the
[development guide](../../docs/Development.md).

| Component | Build helper |
| --- | --- |
| Symbian^3 video and surface headers | `scripts/apply-sdk-patches.py` |
| ARMv6 allocator | `scripts/rebuild-sdk-runtime.py` |
| C99 math functions | `scripts/rebuild-sdk-math.py` |
| LLVM double-complex helpers | `scripts/rebuild-sdk-complex.py` |
| Linux resource compiler | `scripts/rebuild-sdk-rcomp.py` |
| IP certificate identities and SNI | `scripts/rebuild-sdk-mbedtls-ip.py` |
| ScreenPlay headers and import proxies | `scripts/rebuild-sdk-screenplay.py` |

Paths in this table are relative to `Tools/Symbian`. The helpers use
`SYMBIAN_SDK_PREFIX` for the SDK and `SYMBIAN_WEBKIT_WORK` for build output.
Run the native patch-stack build before the allocator, math and complex
rebuilds, in that order. Each rebuild checks its output before installation;
`--install` selects installation for the runtime, math, complex, resource
compiler and TLS helpers. `build-browser.sh` installs the ScreenPlay interfaces.

## Symbian^3 interfaces

`apply-sdk-patches.py` exports `videoplayer2.h`, `surfaceeventhandler.h` and
`graphics/surface.h`. The SDK's video import proxy supplies
`CVideoPlayerUtility2`, including display-window, extent, clipping, rotation
and scale controls. Audio and video utilities expose resource-notification
and playback-resume APIs.

The original headers come from the SDK's recorded Symbian sources:

- Multimedia Audio: `a39c075cce88671b23d13598fda2f7cbf008f3bb`.
- Graphics: `ff133bc50e6158bfb08cc093b0f0055321dcde99`.

The manifests contain source paths and content identities. Original copyright,
Eclipse Public License notices and feature guards are retained.

Component details: [allocator](0001-mimalloc-32bit-pagemap.patch),
[math](math/README.md), [complex](complex/README.md),
[resources](rcomp/README.md), [TLS](mbedtls-ip/README.md), and the
[native patch stack](../symbian-native/README.md).
