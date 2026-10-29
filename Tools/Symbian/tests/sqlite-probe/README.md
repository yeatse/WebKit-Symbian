# SQLite guest regression

This standalone E32 executable links the same SQLite archive as the browser. It checks directory-lock exclusion between two connections, a successful write after releasing the lock, rollback readback on the original connection, and committed data after close/reopen. It also records default-VFS failures, POSIX lock support, and Open C's `O_BINARY == O_BUFFERED` behavior to distinguish platform failures from SQLite failures.

Only `C:/Data/WebKit/sqlite-probe*` files are used. The probe never opens the browser's `cookies.db` or CA bundle. Diagnostic default-VFS errors and the explicitly buffered POSIX experiment are expected on this SDK; the final PASS requires both `unix-dotfile` cases to pass. No explicit journal synchronization is inserted before rollback.

Build with the SDK, patched converter, and the dependency archive used for the browser:

```sh
cmake -S Tools/Symbian/tests/sqlite-probe -B "$SYMBIAN_WEBKIT_WORK/sqlite-probe-regression" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$SYMBIAN_SDK_PREFIX/cmake/symbian-arm.cmake" \
  -DSYMBIAN_SDK_PREFIX="$SYMBIAN_SDK_PREFIX" \
  -DSQLITE_DEPENDENCY_PREFIX="$SYMBIAN_WEBKIT_WORK/deps/install" \
  -DSYMBIAN_NATIVE_CONVERTER="$SYMBIAN_WEBKIT_WORK/symbian-native/cpp/symbian/tools/symbian-native" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$SYMBIAN_WEBKIT_WORK/sqlite-probe-regression" --parallel 1
```

Use the installed firmware's `XDG_*`, `DISPLAY`, `EKA2L1`, and `EKA2L1_DATA` settings. The console guest exits normally; the host invocation must also exit zero. The bounded command below holds both existing emulator locks, rotates only this probe's previous log, and rejects a stale/missing result or panic. A timeout is a failure, even if partial PASS lines exist.

```sh
(
  set -eu
  data=${EKA2L1_DATA:-$XDG_DATA_HOME/EKA2L1/data}
  exec 8>"$data/.jsc-run.lock"; flock -n 8
  exec 9>"$data/.browser-run.lock"; flock -n 9
  mkdir -p "$SYMBIAN_WEBKIT_WORK/runs" "$data/drives/c/Data/WebKit" "$data/drives/c/sys/bin"
  run=$(mktemp -d "$SYMBIAN_WEBKIT_WORK/runs/sqlite-probe-XXXXXXXX")
  guest="$data/drives/c/Data/WebKit/sqlite-probe.log"
  if test -e "$guest"; then mv "$guest" "$run/previous-probe.log"; fi
  cp "$SYMBIAN_WEBKIT_WORK/sqlite-probe-regression/e32/sqlite_probe.exe" "$data/drives/c/sys/bin/sqlite_probe.exe"
  emulator=$(realpath "$EKA2L1")
  status=0
  (cd "$(dirname "$emulator")"
    timeout --signal=TERM --kill-after=3s 30s "$emulator" --device "${EKA2L1_DEVICE:-RM-707}" \
      --run 'C:\sys\bin\sqlite_probe.exe') >"$run/emulator.log" 2>&1 || status=$?
  if test -f "$guest"; then cp "$guest" "$run/sqlite-probe.log"; fi
  printf 'Logs: %s\n' "$run"
  test "$status" -eq 0
  test -s "$run/sqlite-probe.log"
  grep -qx 'PASS: SQLite dotfile locking, rollback and persistence' "$run/sqlite-probe.log"
  grep -qx 'sqlite-probe exiting 0' "$run/sqlite-probe.log"
  ! grep -Eiq 'FAIL:|panicked|access violation|KERN-EXEC|USER-EXEC|segmentation fault' "$run/sqlite-probe.log" "$run/emulator.log"
)
```

With the fixed archive, the one-page rollback journal is 4616 bytes before rollback, and the same connection reads `persisted` immediately after rollback. An archive that uses Open C's buffered `O_BINARY` leaves the last four-byte checksum pending: the journal reports 4612 bytes and the immediate readback fails, even though reopening the database reads the original disk contents. Host SQLite checks alone cannot detect this guest descriptor behavior. These checks cover ordinary commits and rollback, not power-loss recovery.
