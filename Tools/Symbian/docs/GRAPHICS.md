# Symbian graphics and WebGL implementation

Status recorded on 2026-10-29: the EKA2L1 browser passed **10/10 compositing** and
**9/9 WebGL** checks, with native EGL/GLES2 initialization and successful
360 × 532 presentation recorded separately for each page. Both browser drivers
completed their observation periods and normal host shutdown, with no panic or
compositor fallback; the installed CA file was unchanged. The repaired browser's
six promoted/plain text regions also match the software reference exactly, and
both one-pixel stripe rows have zero RGB error. Original logs, screenshots and
hashes are in [browser-final](validation-graphics/browser-final/README.md) and
[compositing-text](validation-graphics/compositing-text/README.md).

On the same final emulator, the standalone native EGL probe passed **131/131**
checks (nine exact pixel checks and three swaps), and the ANGLE probe passed
**45/45**, including real depth rendering and bounded-read rejection without
buffer or guard writes. Both standalone guests and hosts exited zero. Earlier
123/123 EGL and 34/34 or 40/40 ANGLE results remain separate historical records.

These are EKA results, not physical-device GPU or performance measurements, a
full WebGL conformance result, or proof of a particular JavaScript tier. Browser
media integration remains pending; its native MMF results are documented
separately in [MEDIA.md](MEDIA.md).

## Rendering boundaries

WebGL1 uses the bundled ANGLE frontend, validation, shader translator and GLES
renderer. `FunctionsEGLSymbian` opens the firmware `libegl.dll` and
`libglesv2.dll` with `RLibrary`, resolving the SDK import-contract ordinals.
`generate_symbian_exports.py` extracts ordinals from actual proxy function bodies;
the generated table records both proxy hashes. Extension resolution falls back
to the native `eglGetProcAddress` after core ordinal lookup.

ANGLE exposes private `EGL_*` and `GL_*` entry points to WebCore. Its public
`egl*`/`gl*` aliases are excluded, while the native TextureMapper compositor uses
firmware imports. `USE_ANGLE_EGL` stays OFF; enabling WebGL does not replace the
native compositor with ANGLE. The standalone archives contain 963 private entry
points, zero public native aliases and zero unsupported ELF TLS references.

The Symbian SDK's native EGL handles are integers. Common WebCore and ANGLE
headers retain the Khronos pointer types; the separate native EGL bridge makes
the explicit 32-bit conversions. ANGLE's loader has compile-time handle-size
checks and calls the real native API using the ARM base calling convention.
ANGLE thread state uses pthread keys with per-thread allocation and destruction,
since this Symbian runtime does not provide ELF `thread_local` support.

`GraphicsContextGLSymbianANGLE` requests an ES2 WebGL-compatible context, robust
resource initialization, no client vertex arrays and no implicit object creation
on bind. It rejects WebGL2. It renders to ANGLE-managed framebuffer storage and
copies a snapshot for Cairo/native TextureMapper presentation, including channel
and alpha conversion. This has a readback/upload cost; it is not zero-copy GPU
sharing. Readback boundaries restore the native compositor context before its
next texture upload. The separate context ownership regression record is in
[validation-graphics/context](validation-graphics/context/README.md).

The tested EKA EGL implementation explicitly rejects nonzero native
`eglCreateContext(..., share_context, ...)` with `EGL_BAD_PARAMETER`. The ANGLE
probe's two contexts use ANGLE virtualization over its renderer context; their
successful state isolation does **not** demonstrate native EGL context sharing.
The CPU snapshot path does not require sharing native GL objects with ANGLE.
Native compositor and ANGLE displays both reach the native default display, so
per-WebGL-context destruction does not terminate that shared native display.

## Dependencies and actual validation

The tested SDK is 0.3 with Clang 23.1.2. ANGLE is compiled as ARMv6 ARM state,
VFPv2 instructions and base AAPCS (`-mfloat-abi=softfp`), not hard-float PCS.
The E32 converter needs patches `0001`–`0003` under
[patches/symbian-native](../patches/symbian-native/README.md). Patch `0003`
accepts the explicit ARMv6/VFPv2 attributes and sets E32 VFP metadata while
retaining incompatible CPU/FP/PCS rejection. Do not strip those attributes or
compile soft-float merely to bypass a converter error. The recorded ANGLE image
has E32 flags `0x12100028`.

The tested EKA base is `5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a`, with the
repository's ordered patches `0001`, `0005`–`0016`. Patches `0006`–`0008`
cover teardown and filesystem behavior used by the runner. `0009` implements
EGL attribute termination and real GLES capability/object queries and program
validation. `0010` is present in the tested image for native media; an ANGLE
probe pass does not validate its media features. `0011`–`0014` fix failures found
by the native probe, and `0016` fixes depth-only framebuffer handling found by
ANGLE. `0015` concerns native audio. Their complete sequential application was
checked against the fixed base; all 13 current patch hashes match the recorded
[patch-chain check](validation-graphics/native-angle/depth-after/patch-chain.json).

| Layer | Recorded result | What it demonstrates |
| --- | --- | --- |
| Native EGL/GLES2 | 131/131 | Pbuffer/window contexts, real shaders, capability/object/current-state queries, program validation, nine exact pixel checks, alpha blending, three swaps and surface recreation after native window resize |
| ANGLE over firmware GLES2 | 45/45 | Real renderer, WebGL-compatible context, exact shader/depth pixels, virtual-context and pthread state isolation, invalid-call rejection and bounded-read buffer/guard checks |
| GLAD host loader regression | 7/7 | Capability-gated precision API loading, exact call forwarding, missing resolver rejection and clearing stale pointers |
| Browser compositing page | 10/10 | Transform, opacity, clipping, scrolling, repaint, resize and animation checks, plus separate native presentation evidence |
| Browser WebGL page | 9/9 | WebGL 1 storage, shader/texture/FBO queries, validation, resize, canvas uploads/presentation and context loss/restoration |
| Browser text/stripe comparison | Exact match | Six 280 × 24 text regions match the software reference; both one-pixel stripe rows have zero RGB error |

The final host SHA-256 is
`ff218e2e687c183c993aad2245824354e726f4c8bbefdff9b093b604602e18b1`.
The native image SHA-256 is
`bd8fce0b249e73edbe0eb9661b1d8d6a39b9adafa0b2340981e5ae1a5a1c9aa3`;
the ANGLE image SHA-256 is
`8148b8ff1b50acfc8bac5ef83b4ca37561742c3f163fc1dd27fff5c87ee19a33`;
the browser image SHA-256 is
`8ba0a5068a3dbd3aef616d71ae3b9f6d6b0a906c376c49fa4a9cc04685eaead5`.
The final native EGL rerun is in `validation-graphics/native-egl/final-clean`;
the host has no temporary `MMFSTART` diagnostic instrumentation. Earlier runs
retain their original executable hashes rather than inheriting these identities.
The guest identifies the renderer as ANGLE over EKA2L1's GLES2 device. These
strings are diagnostic identity, not proof of physical PowerVR execution.

## Failures found and corrected

1. Native shader precision queries crashed the host. The desktop GLAD loader
   had not loaded `glGetShaderPrecisionFormat`, while a check of the debug wrapper
   mistook the wrapper for a valid underlying pointer. Patch `0011` loads the
   five ES2-compatibility entry points when desktop GL 4.1 or
   `GL_ARB_ES2_compatibility` actually provides them, checks the underlying
   pointer, and forwards real errors. Missing support fails explicitly; no GPU
   precision values are fabricated. The corrected guest queried all 12
   shader/type precision combinations without a crash.
2. Framebuffer `OBJECT_NAME` queries failed because generated texture and
   renderbuffer objects had no initialized guest name. Patch `0012` initializes
   the field and assigns actual guest names at object creation, also covering
   buffers and framebuffers. Host GL handles are not exposed as guest names.
3. Reading the default window framebuffer queued `GL_INVALID_OPERATION` before
   a valid read. A named-framebuffer lookup was incorrectly applied to binding
   zero. Patch `0013` performs object lookup only for nonzero bindings and uses
   the existing EGL surface read path for the default framebuffer. The unchanged
   native probe subsequently passed all nine pixel checks and all three swaps.
4. ANGLE initially returned `EGL_NO_DISPLAY` before backend initialization.
   The Symbian client-extension list omitted
   `EGL_ANGLE_platform_angle_device_type_egl_angle`, so frontend validation
   rejected the backend's required platform attribute. Adding the implemented
   capability to `Display.cpp` allowed the same real backend to initialize and
   complete the 34 checks.
5. The browser's first native TextureMapper frame produced
   `GL_INVALID_ENUM`. `beginPainting()` queries `GL_VIEWPORT`, which EKA had
   not implemented; the earlier native probe queried only
   `GL_MAX_VIEWPORT_DIMS`. Patch `0014` returns the actual guest viewport state,
   including its origin, without leaking the driver's scale or Y inversion.
   The expanded native probe then passed 131 checks. The subsequent browser
   frame presented successfully; WebGL canvas creation is a separate gate.
6. Once the browser enabled WebGL, ANGLE incorrectly advertised
   `GL_OES_depth24` on all GLES2 drivers. WebCore consequently chose a depth
   format the native API did not support. The ANGLE format table now requires
   GLES3 or the actual `GL_OES_depth24` extension for 24-bit renderbuffer support.
   The native implementation's rejection was correct and was not bypassed.
7. Real depth rendering then exposed an EKA null dereference when ANGLE cleared
   a depth-only scratch framebuffer for robust resource initialization. Patch
   `0016` handles creation without a color attachment and valid null attachment
   transitions. The expanded ANGLE probe passes all 40 checks with this patch;
   resource initialization and depth tests remain enabled.
8. Browser readback used `ReadnPixelsRobustANGLE`, whose base command additionally
   requires GLES3.2 or `EXT/KHR_robustness`. The GLES2 backend has no such native
   capability. Symbian WebCore now explicitly requires
   `GL_ANGLE_robust_client_memory` and uses `ReadPixelsRobustANGLE` for all three
   readback entry points. This retains ANGLE's destination-size, packing, format
   and framebuffer validation and the same underlying read operation. It does
   not fall back to unbounded `glReadPixels`. The extended 45/45 probe verifies
   both rejection without memory writes and successful bounded pixel output.
9. Promoted text and one-pixel stripes were blurred even though all ten DOM and
   geometry checks passed. A bounded diagnostic recorded page/device scale 1,
   but promoted backings retained the initial `360 / 980` raster scale. A
   280 × 17 layer was uploaded at 102 × 6 pixels and magnified by 2.72222.
   TextureMapper inherited an empty scale-change callback and did not dirty
   these otherwise unchanged layers. Its override now marks the complete layer
   dirty, updating scale and rebuilding/rasterizing all tiles. Temporary
   diagnostics were removed; fixture styles and texture filters are unchanged.
   The repaired browser's six text regions and two stripe rows match the
   software reference exactly.

The failed and successful guest logs are retained, without changing their
expected values. The first precision-crash probe predates the command-line
tokenizer correction; its requested arguments are therefore not evidence that
they were consumed. The final native log explicitly records the effective
15,000 ms hold and ES3 rejection option.

## Reproduction and evidence

Build instructions and strict success conditions are in
[tests/egl-probe](../tests/egl-probe/README.md) and
[tests/angle-probe](../tests/angle-probe/README.md). Use an isolated EKA data
directory with an installed firmware and the patched host. The finite runner
`eka2l1/run-native-probe.py` takes explicit image/host hashes, enforces a deadline,
closes the emulator normally to flush file buffers and rejects forced
termination, nonzero exits or panics. Do not concurrently share its emulator
data directory with a browser/JSC run.

The [native EGL evidence](validation-graphics/native-egl/README.md) and
[ANGLE evidence](validation-graphics/native-angle/README.md) contain unchanged
result records, original guest-log bytes compressed with gzip, source hashes,
failure records and the resized native-window screenshot. No executable, SDK,
firmware or duplicate host log is vendored. Host-log hashes and exit summaries
identify the original runs; host logs containing machine paths are kept outside
those standalone-probe archives.

The [strict browser runner](../tests/browser-graphics-runner.md) separately
requires current page results, successful native presentation, no fallback,
fresh screenshots and normal driver completion. Its [browser evidence](validation-graphics/browser-final/README.md)
includes original guest, host and launcher logs as well as screenshots. Browser
native-surface resize, input-panel overlay and fallback transitions still need
separate observations; the page's layout-resize check and the standalone EGL
window-resize check do not establish those browser behaviors.
