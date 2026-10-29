# ARM11 upstream JSC smoke evidence

The 20 selected upstream stress tests passed on QEMU ARM1136 with the existing
ARMv6 softfp JSC Linux ELF and its default options. The recorded run took
13.666 seconds, with no timeout or failed assertion. Tests ran sequentially,
with all guest threads pinned to one host CPU and a 30-second process deadline.

| Area | Tests | Result |
| --- | ---: | --- |
| Arguments, varargs, tail calls | 6 | PASS |
| Object and array conversions | 4 | PASS |
| RegExp semantics | 4 | PASS |
| Division, modulo, clz32 | 6 | PASS |

This is a bounded regression sample, not the full upstream suite. DFG was
enabled, but the unmodified tests do not prove that every function entered DFG.
`useRegExpJIT=0`, so the RegExp results cover semantics and JSC call boundaries,
not Yarr-generated machine code. The separate tier matrix and concurrent DFG
smoke provide evidence of actual DFG body execution.

## Recorded artifacts

- `sources.json`: fixed upstream commit, original source paths, Git blob IDs,
  SHA-256 digests, byte counts, categories, and runner comments for all 20 tests.
- `results.json`: original exit codes, durations, option preflight, executable
  hashes, and log hashes. Only machine-specific paths were replaced with the
  symbolic parameters used below; command argument order and values are intact.
- `logs/`: byte-for-byte guest stdout/stderr logs. All 20 test logs are empty
  because these upstream tests succeed silently; `preflight.log` records flags.
- `reproduce.py`: restores this fixed sample from Git, verifies both source
  identities, and runs it without overriding JSC options.

The recorded ELF SHA-256, unchanged before and after the run, is
`7eaf26301b3818cee28177a291cd84928099d7876e89e32507175cafca3e6372`.
QEMU was version 10.0.13. No test source or executable is vendored here.

The actual default profile was:

```json
{"useJIT":1,"useBaselineJIT":1,"useDFGJIT":1,"useFTLJIT":0,"useConcurrentJIT":1,"numberOfDFGCompilerThreads":2,"useRegExpJIT":0,"useSamplingProfiler":0,"testLoopCount":10000}
```

Each selected file contains its own assertions and needs no external helper,
DOM, worker, or Wasm. `noInline`, `OSRExit`, `edenGC`, and `testLoopCount` are
native JSC shell facilities. One varargs test requests no sampling profiler on
Linux; the recorded default already satisfies that requirement. A modulo test
skips Windows; that condition does not apply to the Linux ELF. These runner
comments remain unchanged and are preserved in the source manifest.

## Reproduce

Use Linux with Python 3, Git, QEMU ARM user mode, an ARM Linux runtime sysroot,
and the port's ARMv6 softfp JSC ELF. Set `CHECKOUT` to a WebKit Git checkout
containing the pinned base commit, `TEST_WORK` to a scratch directory, and
`QEMU_ARM`, `ARM11_LINUX_SYSROOT`, and `JSC_ELF` to the corresponding tools and
artifact. Configure any host runtime libraries needed by your QEMU installation
before running; the guest's `LD_LIBRARY_PATH` and `LD_PRELOAD` are unset explicitly.

```sh
python3 "$CHECKOUT/Tools/Symbian/docs/validation-arm11/upstream-smoke/reproduce.py" \
  --checkout "$CHECKOUT" --work "$TEST_WORK" \
  --qemu "$QEMU_ARM" --sysroot "$ARM11_LINUX_SYSROOT" --jsc "$JSC_ELF"
```

The script creates a new timestamped directory below `TEST_WORK`, restores all
20 originals using `git show`, verifies their Git blob IDs and SHA-256 digests,
and records fresh results there. It uses one allowed host CPU, runs one test at
a time, and kills the whole process group on a 30-second timeout. Concurrent
JIT remains enabled; its compiler thread count is allowed to reflect the host.
The preflight rejects a different default tier/RegExp/sampling profile instead
of silently changing options. The archived evidence is never overwritten.

The underlying restoration and default-profile command for a single case are:

```sh
BASE=4fb33923db2f945803df49546f75867980365c08
SOURCE=JSTests/stress/arity-mismatch-arguments-length.js
mkdir -p "$TEST_WORK/JSTests/stress"
git -C "$CHECKOUT" show "$BASE:$SOURCE" > "$TEST_WORK/$SOURCE"
"$QEMU_ARM" -cpu arm1136 -L "$ARM11_LINUX_SYSROOT" \
  -U LD_LIBRARY_PATH -U LD_PRELOAD "$JSC_ELF" "$TEST_WORK/$SOURCE"
```

Use `reproduce.py` for the recorded affinity, per-process deadline, source
verification, and full result capture. No expectations are edited or shimmed.
