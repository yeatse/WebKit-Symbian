# ANGLE shader and rendering probe

Builds a native probe using the Symbian ANGLE build. It compiles shaders and
checks GLES rendering through the firmware EGL/GLES interfaces.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native
export ANGLE_BUILD_DIR=/path/to/graphics-build
export PROBE_BUILD=/tmp/angle-probe
Tools/Symbian/tests/angle-probe/build.sh
```

Run `e32/angle_probe.exe` from the build directory using the native probe setup in
[Testing](../../docs/TESTING.md). Passing checks compile and link the fixture
shaders and produce their expected pixels.
