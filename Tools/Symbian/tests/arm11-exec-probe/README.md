# ARM11 executable memory probe

This standalone ARMv6 E32 executable compiles the port's actual `SymbianSystem.cpp`, without JavaScriptCore. It allocates the 16 MB JIT code pool, executes generated ARM instructions, rewrites the instructions and calls `User::IMB_Range`, then branches in both directions between the pool's first and last pages. It also checks code-page reuse and preserves the data allocator's alignment, guards, no executable promotion, repeated-commit preservation, range validation, and zero-on-recommit behavior.

Symbian code chunks are RWX and physically committed in full. Their commit/decommit accounting is logical; physical pages are retained until release. They cannot provide guard pages or read-only/no-access protection. Ordinary data continues to use disconnected chunks with real page commitment and decommitment.

Build in a separate work directory using the SDK and the patched native converter:

```sh
cmake -S Tools/Symbian/tests/arm11-exec-probe -B "$SYMBIAN_WEBKIT_WORK/exec-probe" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$SYMBIAN_SDK_PREFIX/cmake/symbian-arm.cmake" \
  -DSYMBIAN_SDK_PREFIX="$SYMBIAN_SDK_PREFIX" \
  -DSYMBIAN_NATIVE_CONVERTER="$SYMBIAN_NATIVE_CONVERTER" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$SYMBIAN_WEBKIT_WORK/exec-probe" --parallel 2
```

Use the installed firmware's `XDG_*`, `DISPLAY`, and `EKA2L1` settings. This command holds the existing emulator locks, removes the previous result from the guest path, and requires all 20 checks, a normal guest exit, and a normal host exit. A timeout or panic fails validation even when partial PASS lines exist.

```sh
(
  set -eu
  data=${EKA2L1_DATA:-$XDG_DATA_HOME/EKA2L1/data}
  exec 8>"$data/.jsc-run.lock"; flock -n 8
  exec 9>"$data/.browser-run.lock"; flock -n 9
  mkdir -p "$SYMBIAN_WEBKIT_WORK/runs" "$data/drives/c/Data/WebKit" "$data/drives/c/sys/bin"
  run=$(mktemp -d "$SYMBIAN_WEBKIT_WORK/runs/exec-probe-XXXXXXXX")
  guest="$data/drives/c/Data/WebKit/arm11-exec-probe.log"
  if test -e "$guest"; then mv "$guest" "$run/previous-probe.log"; fi
  cp "$SYMBIAN_WEBKIT_WORK/exec-probe/e32/arm11_exec_probe.exe" "$data/drives/c/sys/bin/arm11_exec_probe.exe"
  emulator=$(realpath "$EKA2L1")
  status=0
  (cd "$(dirname "$emulator")"
    timeout --signal=TERM --kill-after=3s 30s "$emulator" --device "${EKA2L1_DEVICE:-RM-707}" \
      --run 'C:\sys\bin\arm11_exec_probe.exe') >"$run/emulator.log" 2>&1 || status=$?
  if test -f "$guest"; then cp "$guest" "$run/arm11-exec-probe.log"; fi
  printf 'Logs: %s\n' "$run"
  test "$status" -eq 0
  grep -qx 'PASS: arm11-exec-probe 20/20' "$run/arm11-exec-probe.log"
  grep -qx 'arm11-exec-probe exiting 0' "$run/arm11-exec-probe.log"
  ! grep -Eiq 'FAIL:|panicked|access violation|KERN-EXEC|USER-EXEC|segmentation fault' "$run/arm11-exec-probe.log" "$run/emulator.log"
)
```

Passing in EKA2L1 demonstrates actual guest ARM code execution and the emulator's cache-invalidation path. It does not substitute for physical ARM11 instruction-cache validation or prove JavaScriptCore's JIT compiler correct.
