# Native ANGLE WebGL1 probe

This small Symbian executable links the same `libANGLE.a` and `libGLESv2.a` as
WebCore. It calls ANGLE's private `EGL_*` and `GL_*` entry points. It does not link
native EGL/GLES import proxies: the Symbian ANGLE backend opens those DLLs and
resolves the SDK's verified ordinals. A successful draw therefore exercises the
ANGLE frontend, shader translation, GLES renderer, and native firmware API.

It checks a WebGL-compatible ES2 context with robust resource initialization,
shader compilation/linking, an exact red framebuffer pixel, separate virtual
contexts with restored framebuffer state, rejection of ungenerated texture names
and client vertex arrays, and separate EGL context/error state on a pthread.
The thread barrier has a bounded three-second wait on each side.

Version 2 also follows WebCore's default depth-buffer choice: use 24 bits only
when ANGLE advertises `GL_OES_depth24`, otherwise use the GLES2 core 16-bit
format. It logs the selected enum, requires real storage and framebuffer
completeness, queries the allocated precision, and checks exact pixels with a
fragment first rejected and then accepted by the depth test. An allocation
failure does not retry with a smaller format. This detects an ANGLE capability
claim that the native GLES implementation cannot fulfill.

The current probe additionally exercises `GL_ReadPixelsRobustANGLE`, the bounded
GLES2 entry used by WebCore. A three-byte RGBA destination must be rejected
without changing the destination or either guard byte; a four-byte destination
must return the exact pixel, byte length and dimensions while preserving both
guards. This does not depend on native `EXT/KHR_robustness`, which the separate
`ReadnPixels` base command requires.

Build the Symbian ANGLE targets first, then run from a host environment that can
execute the SDK's Clang/CMake/Ninja binaries:

```sh
export SYMBIAN_SDK_PREFIX=/path/to/symbian-sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/patched/symbian-native
export ANGLE_BUILD_DIR=/path/to/graphics-build
export PROBE_BUILD=/path/to/angle-probe-build
Tools/Symbian/tests/angle-probe/build.sh
```

The converter must support ARMv6/VFPv2 instructions with the base AAPCS calling
convention. The probe retains its VFPv2 ELF attribute. Do not remove the attribute
or compile the probe as soft-float to work around an incompatible converter.
The generated image uses UID `0xe00008f4`, a 64 MiB maximum heap, and a 512 KiB
stack.

Copy `e32/angle_probe.exe` to `C:\sys\bin\angle_probe.exe` in an isolated Symbian
firmware and launch it. It overwrites `C:\Data\WebKit\angle-probe.log`, mirrors
progress through `RDebug`, and exits. A pass requires the fresh log to contain a
`RESULT` with `failed:0`, followed by `PASS: angle-probe` and
`angle-probe exiting 0`, with no guest panic or host failure. When using EKA2L1,
close the host normally before collecting the log so its file buffers are
flushed; a timeout or forced kill is a failure, not a pass.

This probe does not cover WebCore canvas presentation, DOM WebGL API behavior,
WebGL2, hardware performance, or zero-copy context sharing. Use `../webgl.html`
inside the browser for the WebCore integration checks. Native EKA GLES query and
shader entry points must be implemented accurately for the ANGLE caps checks to
succeed; the probe does not substitute fixed capabilities or expected pixels.

The [latest standalone evidence](../../docs/validation-graphics/native-angle/README.md)
records **45/45** on the final `ff218e2e…` host, including depth and bounded-read
checks. The separate [browser record](../../docs/validation-graphics/browser-final/README.md)
passes 9/9 WebGL checks on that host; neither result is a full conformance suite
or a physical-device GPU result.
