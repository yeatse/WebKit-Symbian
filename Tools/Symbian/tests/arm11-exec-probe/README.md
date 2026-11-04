# ARM11 executable memory

Builds a native probe for generated ARM instructions and the production memory
bridge. Select the Symbian SDK and converter before configuring the build.

```sh
cmake -S Tools/Symbian/tests/arm11-exec-probe -B /tmp/arm11-exec-probe -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$SYMBIAN_SDK_PREFIX/cmake/symbian-arm.cmake" \
  -DSYMBIAN_SDK_PREFIX="$SYMBIAN_SDK_PREFIX" \
  -DSYMBIAN_NATIVE_CONVERTER="$SYMBIAN_NATIVE_CONVERTER" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/arm11-exec-probe --parallel 2
```

Run `e32/arm11_exec_probe.exe` from the build directory using the native probe
setup in [Testing](../../docs/TESTING.md). The probe executes generated code,
rewrites and flushes its instruction cache, and reuses executable allocations.
It records `PASS: arm11-exec-probe 20/20` in
`C:/Data/WebKit/arm11-exec-probe.log` when the checks pass.
