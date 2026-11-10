# Skia compositor and damage probe

Uses the production `SkiaCompositingLayer`, native EGL display and Ganesh
context. Fourteen checks cover GPU composition, partial damage after movement
and removal, preservation of undamaged pixels, a static frame, opacity,
clipping, path masks, filters, preserve-3D ordering, overlapping child reordering
and full repaint of a resized target.

The Canvas display check uses a real accelerated `ImageBuffer`, publishes its
layer delegate, reads the GPU snapshot and checks a redraw through the same
delegate.

The undamaged-region check places a GPU marker outside the changed layer's
bounds. Partial composition preserves it; a new target receives a full repaint.

Build WebCore before building the probe:

```sh
cmake -S Tools/Symbian/tests/skia-compositor-probe -B /tmp/skia-compositor-probe -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$SYMBIAN_SDK_PREFIX/cmake/symbian-arm.cmake" \
  -DWEBKIT_BUILD_DIR="$SYMBIAN_WEBKIT_WORK/build/webcore" \
  -DDEPS_DIR="$SYMBIAN_WEBKIT_WORK/deps/install" \
  -DICU_LIBRARY_DIR="$SYMBIAN_WEBKIT_WORK/icu/install/lib" \
  -DJPEG_LIBRARY="${SYMBIAN_JPEG_PREFIX:-$SYMBIAN_WEBKIT_WORK/deps/install}/lib/libjpeg.a"
ninja -C /tmp/skia-compositor-probe
python3 Tools/Symbian/scripts/e32-header.py \
  /tmp/skia-compositor-probe/e32/skia_compositor_probe.exe \
  0x100000 0x4000000 0x100000 --fpu=vfpv2
```

With the emulator profile configured as in [Testing](../../docs/TESTING.md):

```sh
probe_image=/tmp/skia-compositor-probe/e32/skia_compositor_probe.exe
python3 Tools/Symbian/eka2l1/run-native-probe.py --probe compositor \
  --image "$probe_image" --image-sha256 "$(sha256sum "$probe_image" | cut -d' ' -f1)" \
  --emulator "$EKA2L1" --emulator-sha256 "$(sha256sum "$EKA2L1" | cut -d' ' -f1)" \
  --reports /tmp/skia-compositor-runs
```

The result is written to `C:/Data/WebKit/skia-compositor-probe.log`.
A passing run reports `PASS: skia-compositor-probe` and
`skia-compositor-probe exiting 0`.
