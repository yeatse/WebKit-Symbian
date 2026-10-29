# Native Symbian EGL/GLES2 window probe

This standalone probe does not link WebKit or ANGLE. It imports the firmware's `libegl.dll` and `libglesv2.dll`, creates a real `RWindow`, and requests an ES2 RGBA8/stencil8 window configuration matching WebCore's current TextureMapper minimum. Missing support fails explicitly; there is no software drawing fallback.

Build from the checkout after selecting the SDK and E32 converter:

```sh
export SYMBIAN_SDK_PREFIX=/path/to/sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native
export SYMBIAN_WEBKIT_WORK=/path/to/build-work
Tools/Symbian/tests/egl-probe/build.sh
```

The SDK uses ARMv6 base AAPCS with software floating point for this native probe. The image includes ARM-state probe code and SDK Thumb-1 runtime code; it is not a JSC tier test. Defaults can be overridden with `SYMBIAN_SDK_PREFIX`, `SYMBIAN_NATIVE_CONVERTER`, `SYMBIAN_WEBKIT_WORK`, and `PROBE_BUILD`. The build script updates the E32 header CRC after selecting a 64 KB initial / 16 MB maximum heap and 256 KB stack.

The probe compiles and links a textured GLES2 shader, renders four independently checked texels into an RGBA texture FBO, checks premultiplied alpha blending over blue, samples that FBO texture on the window, and verifies window pixels. It also queries shader precision (allowing optional fragment highp to be unavailable), renderbuffer storage, FBO attachments, vertex attribute values/state/pointers, and validates linked versus unlinked programs. TextureMapper state checks cover current viewport and scissor rectangles with nonzero origins, enable states, and the current program/framebuffer. These differ from maximum viewport limits; an earlier EKA implementation passed the limits query while rejecting `GL_VIEWPORT`. It also requires positive viewport dimensions, cube-map/renderbuffer/uniform/varying limits and aliased point/line ranges needed by ANGLE. A separate pbuffer uses a standard `EGL_NONE`-terminated attribute list. It presents two distinct frames, resizes the actual native window, recreates only its EGL surface while preserving the GL context/resources, then checks a pixel near the new edge and presents a third frame. This is surface recreation on resize, not a claim that the driver automatically resizes an existing surface.

For EKA/device execution, deploy `$SYMBIAN_WEBKIT_WORK/build/egl-probe/e32/egl_probe.exe` to `C:\sys\bin\egl_probe.exe` and start it with optional arguments `--hold-ms 15000` (default 10000; accepted range 0–60000). Only when testing EKA's declared ES2-only implementation, add `--expect-es3-rejection` to require an ES3 context request to fail with `EGL_BAD_MATCH`. Do not require that rejection on physical devices that may support ES3. The probe writes a fresh `C:\Data\WebKit\egl-probe.log` and mirrors diagnostics through RDebug. Capture the window while the log reports `READY: egl-window-probe`; its final resized frame is opaque purple. After the bounded hold it releases GL/EGL/window resources and exits.

The SDK startup currently supplies only a synthetic `argv[0]`, so version 2 reads the real process command line using `User::CommandLine()`. Both quoted runner arguments and unquoted options are supported. The `BEGIN` line and `RESULT` report the effective hold time and ES3-rejection option; for the EKA invocation above require `holdMilliseconds=15000` and `expectES3Rejection=true`, not merely that those arguments appear in the host command.

Require exit zero, exactly one `RESULT` record with `failed=0`, `swaps=3`, `pixelChecks=9`, the final `PASS: egl-window-probe` and no FAIL/GL ERROR/EGL ERROR or emulator panic. Driver strings alone never produce PASS. Preserve the executable/source hashes, complete logs, screenshot, emulator revision and actual device/firmware identity. EKA exercises the guest EGL/GLES dispatch and presentation path; physical-device GPU support needs separate execution evidence.

Building this probe alone establishes no emulator or physical-device result. Do not use the browser runner for this executable: it expects a long-lived AVKON browser and has different log/exit rules. Coordinate the shared EKA XDG environment with its current owner before running.

The recorded EKA invocation with ES3 rejection enabled passes **131/131**, including nine pixel checks and three swaps, on the final `ff218e2e…` host. Full image/host hashes, effective arguments, original logs and screenshot are in the [native EGL evidence](../../docs/validation-graphics/native-egl/README.md); `final-clean/` is the latest run. Physical-device support still requires its own execution evidence.
