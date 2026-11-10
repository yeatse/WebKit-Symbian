# Ganesh GLES2 raster probe

Creates a native EGL GLES2 pbuffer and a Skia Ganesh render target. Checks GPU
rectangles, clipping, alpha blending, paths, text and readback, then attaches the
rendered texture in a shared GLES2 context and reads its pixels. It also wraps a
native compositor texture and checks asymmetric top and bottom rows across the
shared contexts.

The context-switch flow checks the first surface's default viewport and scissor,
sets custom values, switches contexts and verifies that the values are preserved.

The shared-context flow closes the creator, then relinks its shader program,
updates its texture and buffer, draws into its renderbuffer and releases the
resources through the surviving context.

The text check uses the browser's installed font configuration.

Build the `Skia` and `WTF` targets first:

```sh
cmake -S Tools/Symbian/tests/ganesh-probe -B /tmp/ganesh-probe -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$SYMBIAN_SDK_PREFIX/cmake/symbian-arm.cmake" \
  -DWEBKIT_BUILD_DIR="$SYMBIAN_WEBKIT_WORK/build/webcore" \
  -DDEPS_DIR="$SYMBIAN_WEBKIT_WORK/deps/install" \
  -DJPEG_LIBRARY="${SYMBIAN_JPEG_PREFIX:-$SYMBIAN_WEBKIT_WORK/deps/install}/lib/libjpeg.a"
ninja -C /tmp/ganesh-probe
python3 Tools/Symbian/scripts/e32-header.py /tmp/ganesh-probe/e32/ganesh_probe.exe \
  0x100000 0x2000000 0x80000 --fpu=vfpv2
```

With the emulator profile configured as in [Testing](../../docs/TESTING.md):

```sh
probe_image=/tmp/ganesh-probe/e32/ganesh_probe.exe
python3 Tools/Symbian/eka2l1/run-native-probe.py --probe ganesh \
  --image "$probe_image" --image-sha256 "$(sha256sum "$probe_image" | cut -d' ' -f1)" \
  --emulator "$EKA2L1" --emulator-sha256 "$(sha256sum "$EKA2L1" | cut -d' ' -f1)" \
  --reports /tmp/ganesh-runs
```

The native result is written to `C:/Data/WebKit/ganesh-probe.log`. A passing run
reports 31 checks, `PASS: ganesh-probe` and `ganesh-probe exiting 0`.
