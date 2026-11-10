# Skia raster probe

Paints through the ARMv6 Skia library into a BGRA buffer and checks rectangle,
clip, alpha and path pixels, plus allocation capacity. Build the `Skia` and `WTF` targets first.

```sh
cmake -S Tools/Symbian/tests/skia-probe -B /tmp/skia-probe -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$SYMBIAN_SDK_PREFIX/cmake/symbian-arm.cmake" \
  -DWEBKIT_BUILD_DIR="$SYMBIAN_WEBKIT_WORK/build/webcore" \
  -DDEPS_DIR="$SYMBIAN_WEBKIT_WORK/deps/install" \
  -DJPEG_LIBRARY="${SYMBIAN_JPEG_PREFIX:-$SYMBIAN_WEBKIT_WORK/deps/install}/lib/libjpeg.a"
ninja -C /tmp/skia-probe
```

Run `e32/skia_probe.exe` in the emulator or on a phone. The result is written to
`C:/Data/WebKit/skia-probe.log`. A passing run reports six checks,
`PASS: skia-probe` and `skia-probe exiting 0`.

With the emulator profile configured as in [Testing](../../docs/TESTING.md):

```sh
probe_image=/tmp/skia-probe/e32/skia_probe.exe
python3 Tools/Symbian/eka2l1/run-native-probe.py --probe skia \
  --image "$probe_image" --image-sha256 "$(sha256sum "$probe_image" | cut -d' ' -f1)" \
  --emulator "$EKA2L1" --emulator-sha256 "$(sha256sum "$EKA2L1" | cut -d' ' -f1)" \
  --reports /tmp/skia-runs
```
