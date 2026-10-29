# Symbian graphics and WebGL implementation

Status recorded on 2026-10-29: the standalone native EGL/GLES2 probe passed
123/123 checks and the real ANGLE probe passed 34/34 in EKA2L1. Both guest
processes and the emulator exited zero, with no reported guest panic or access
violation. Browser WebGL presentation and DOM integration are **not yet
validated** by these results. Physical-device rendering and performance also
remain untested.

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
repository's ordered patches `0001`, `0005`–`0013`. Patches `0006`–`0008`
cover teardown and filesystem behavior used by the runner. `0009` implements
EGL attribute termination and real GLES capability/object queries and program
validation. `0010` is present in the tested image for native media; an ANGLE
probe pass does not validate its media features. `0011`–`0013` fix failures found
by the native probe, described below. Their complete sequential application was
checked against the fixed base. Source and patch hashes are preserved beside
the evidence.

| Layer | Recorded result | What it demonstrates |
| --- | --- | --- |
| Native EGL/GLES2 | 123/123 | Pbuffer/window contexts, real shaders, capability/precision/object queries, program validation, nine exact pixel checks, alpha blending, three swaps and surface recreation after native window resize |
| ANGLE over firmware GLES2 | 34/34 | Real ANGLE renderer and WebGL-compatible context, exact shader pixel, virtual-context state recovery, pthread context/error isolation and invalid-call rejection |
| GLAD host loader regression | 7/7 | Capability-gated precision API loading, exact call forwarding, missing resolver rejection and clearing stale pointers |
| Browser WebGL page | Pending | `tests/webgl.html` must run inside the final browser; host Chromium fixture checks are not Symbian acceptance |

The final host SHA-256 is
`1e418ff2d4af12755a62f6093b2bba79da1248c111336e760f559f50888ae42b`.
The native image SHA-256 is
`c9b8331f218694cc00321f5c772d735713b067cfbc19598af9f5dfedfddacaf6`;
the ANGLE image SHA-256 is
`e098ef9b7b753f01ab787acf496e6fd877d5b1e710fd58f6ee4301dd891685b2`.
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
the repository.
