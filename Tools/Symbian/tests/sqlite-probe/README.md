# Native SQLite persistence

Builds a Symbian SQLite probe using the browser's dependency archive. Select the
SDK, converter and dependency installation before configuring the build.

```sh
cmake -S Tools/Symbian/tests/sqlite-probe -B /tmp/sqlite-probe -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$SYMBIAN_SDK_PREFIX/cmake/symbian-arm.cmake" \
  -DSYMBIAN_SDK_PREFIX="$SYMBIAN_SDK_PREFIX" \
  -DSQLITE_DEPENDENCY_PREFIX=/path/to/deps/install \
  -DSYMBIAN_NATIVE_CONVERTER="$SYMBIAN_NATIVE_CONVERTER" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/sqlite-probe --parallel 2
```

Run `e32/sqlite_probe.exe` from the build directory with the native probe setup in
[Testing](../../docs/TESTING.md). Passing checks create and reopen records,
coordinate database locks and preserve committed data through rollback.
