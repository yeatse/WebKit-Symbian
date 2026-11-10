# Native EGL and GLES2

Creates a native window, renders shader and texture fixtures, presents frames and
resizes the EGL surface while retaining its GL resources.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native
export PROBE_BUILD=/tmp/egl-probe
Tools/Symbian/tests/egl-probe/build.sh
```

Run `e32/egl_probe.exe` from the build directory with the native probe setup in
[Testing](../../docs/TESTING.md). The probe writes `C:/Data/WebKit/egl-probe.log`
and displays its final purple frame during the hold interval.

Passing results include the expected shader pixels, three swaps and
`PASS: egl-window-probe`, followed by normal process exit.
