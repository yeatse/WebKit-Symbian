# ARM11 JavaScript performance

The ARM11 DFG lowers int32 remainder by constant powers of two from 2 through 2^30 to
shifts and a mask. Signed results, negative zero and double fallback are covered
by the [tier suite](../tests/arm11-jit.js). Symbian also enables the existing
RegExp JIT. The [ARM Yarr prologue](../../../Source/JavaScriptCore/yarr/YarrJIT.cpp)
preserves the caller's callee-saved registers.
Patterns unsupported by the ARM RegExp JIT retain the interpreter fallback.

## Native workloads

Use the emulator environment in [Testing](TESTING.md#emulator-setup). Set
`E32_BUILD` to the image build directory and `ICU_DIR` to the ICU build directory.
`run.sh` stages the selected `jsc.exe`, ICU data and all canonical test scripts.
Keep the default concurrent JIT and compilation policy for these workloads.

```sh
TIMEOUT=60 Tools/Symbian/eka2l1/run.sh --useDollarVM=true \
  --useRegExpJIT=false C:/Data/arm11-regexp-benchmark.js -- dfg
TIMEOUT=60 Tools/Symbian/eka2l1/run.sh --useDollarVM=true \
  --useRegExpJIT=true C:/Data/arm11-regexp-benchmark.js -- dfg
TIMEOUT=60 Tools/Symbian/eka2l1/run.sh --useDollarVM=true \
  --useJIT=true --useBaselineJIT=true --useDFGJIT=true --useFTLJIT=false \
  C:/Data/arm11-modulo-benchmark.js -- dfg
```

The [RegExp workload](../tests/arm11-regexp-benchmark.js) measures `test`,
capturing `exec` and alternative replacement with 8-bit and 16-bit subjects.
Its semantic corpus also covers Unicode, backreferences and `lastIndex`.
The [remainder workload](../tests/arm11-modulo-benchmark.js) measures constant
divisors 2, 256 and 1073741824, with signed and negative-zero checks outside timing.

| Workload | Iterations per sample | Samples |
| --- | ---: | ---: |
| RegExp `test`, each subject width | 524288 | 5 |
| RegExp `exec`, each subject width | 131072 | 5 |
| RegExp `replace`, each subject width | 65536 | 5 |
| Each remainder divisor | 4194304 | 3 |

Each script prints `BENCH_RESULT` with timing samples, a semantic digest,
effective options and actual DFG body/compilation evidence, followed by its PASS
line. The driver requires `jsc exiting 0` and saves fresh logs under
`$SYMBIAN_WEBKIT_WORK/runs`. Use matching digests and medians from repeated fresh
processes, alternating baseline/candidate order. Compare RegExp off/on on the
same image; compare remainder changes using images built with identical flags.

## Execution checks

The 13-case suite checks LLInt, Baseline and DFG separately, including remainder:

```sh
python3 Tools/Symbian/eka2l1/run-jit-tests.py \
  --image "$E32_BUILD/e32/jsc.exe" --icu-data "$ICU_DIR/install/share/icu/icudt70l.dat" \
  --emulator "$EKA2L1" --data "$EKA2L1_DATA" \
  --reports "$SYMBIAN_WEBKIT_WORK/jit-runtime" --timeout 60
```

Expect `PASS: arm11-jit 13/13` per profile and matching semantic results. This
suite uses synchronous compilation and reduced thresholds to verify execution;
the benchmark workloads use the normal concurrent policy. Both compilation
and execution markers are checked.

## Measurements

EKA2L1 `b630b7c`, ARMv6/VFPv2 softfp E32, `-Os`, default concurrent tiering.
Three measured process pairs follow one warmup pair; runs alternate order.
Values are medians in milliseconds. Every run checks matching results and DFG execution.

| Integer remainder | Baseline | Optimized | Elapsed reduction |
| --- | ---: | ---: | ---: |
| `% 2` | 1468.74 | 62.48 | 95.7% |
| `% 256` | 1234.36 | 62.50 | 94.9% |
| `% 1073741824` | 312.50 | 46.88 | 85.0% |

The RegExp comparison switches the same image between interpreter and native JIT.

| RegExp workload | Interpreter | JIT | Elapsed reduction |
| --- | ---: | ---: | ---: |
| RegExp `test`, Latin1 | 1296.88 | 156.24 | 88.0% |
| RegExp `test`, UTF-16 | 1406.24 | 187.50 | 86.7% |
| RegExp `exec`, Latin1 | 687.50 | 156.24 | 77.3% |
| RegExp `exec`, UTF-16 | 718.74 | 156.24 | 78.3% |
| RegExp `replace`, Latin1 | 437.50 | 140.62 | 67.9% |
| RegExp `replace`, UTF-16 | 468.74 | 140.62 | 70.0% |

## Next targets

1. Add tag/payload fast paths to [generic 32-bit strict equality](../../../Source/JavaScriptCore/dfg/DFGSpeculativeJIT32_64.cpp).
2. Add a guarded VFP path for positive finite `Math.trunc` inputs in the [native thunk](../../../Source/JavaScriptCore/jit/ThunkGenerators.cpp).
3. Measure compiler queue wait, compilation time and UI frame cost when tuning worker priority, pool size and inlining budgets.
