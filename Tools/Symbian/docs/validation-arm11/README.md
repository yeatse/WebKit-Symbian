# ARM11 validation evidence

`arm11-exec-probe.log` and `exec-probe-result.json` record the standalone executable-memory probe run on 2026-10-29. All 20 checks passed; the guest and EKA2L1 both exited with status zero, without a panic or access violation.

The tested source is [the executable-memory probe](../../tests/arm11-exec-probe/README.md), which documents the SDK build and bounded emulator invocation. It compiles `Source/WTF/wtf/symbian/SymbianSystem.cpp` from this checkout directly. Use a separate `SYMBIAN_WEBKIT_WORK` directory for ARM11 results and the documented emulator locks.

The JSON records the executable and validation-emulator SHA-256 hashes. No executable, firmware, host log, or user-specific host path is included here. This probe establishes code-chunk allocation, actual guest ARM execution and rewriting, 16 MB near branches, and data-page regressions. The separate JSC results below establish execution-tier behavior in the emulator. No physical ARM11 phone or hardware instruction-cache behavior has been validated.

## EKA2L1 JSC tier validation

The [recorded matrix](eka-jsc/result.json) ran the same E32 image under LLInt, Baseline and DFG on 2026-10-29. Each profile passed all 12 cases with normal guest and host exits, no panic or access violation, and identical semantic results. The image SHA-256 is `86fc45d8ccf82c39c3519366f8633c5d1465aeb742dd3ee1404ad8dea5c448a7`. Individual guest logs and result records are in `eka-jsc/llint`, `eka-jsc/baseline`, and `eka-jsc/dfg`.

Observed execution tiers were `[true, false, false]`, `[true, true, false]`, and `[true, true, true]` respectively. DFG compilation counts were 1–4 per kernel, with actual optimized-body execution recorded. The DFG result includes first-call loop OSR entry, explicit OSR exit with live values recovered, and typed-array checks after resizing, growth, and GC.

Upstream `JSVALUE32_64` deliberately exits DFG typed-array access for resizable or growable shared buffers (`UnexpectedResizableArrayBufferView`); only the 64-bit implementation emits their dynamic-length fast path. This suite first proves ordinary typed-array reads execute in DFG, then requires an observed DFG-to-baseline transition with the RAB value preserved. A bounded warm-up accommodates the preceding structure guard. The recorded RAB transition is `before=true`, `after=false`, `value=17`, observed after 221 attempts. Resize/regrow and GSAB/atomic semantics then pass through the supported fallback. This does not claim optimized RAB/GSAB element loads on ARM32.

The same image also passed [default concurrent DFG validation](eka-concurrent/result.json), with one background compiler thread and default compilation thresholds. Both kernels compiled once and executed 128 times in DFG. Two collections started and returned within DFG, all 32 retained roots survived another collection, and the run completed in approximately five seconds. Its guest and host both exited zero without panic. This supplements the deterministic matrix, which explicitly disables concurrent compilation.

The [existing functionality regression](eka-regression/result.json) passed `basic.js`, `es2020.js`, `intl.js`, and `resizable-arraybuffer.js` in one invocation of the same image, with default JIT/DFG/concurrency settings. All four PASS markers, normal guest/host exits, and absence of panic were verified. The existing runner enables SharedArrayBuffer for its RAB/GSAB test.

[The EKA tier runner](../../eka2l1/run-jit-tests.py) uses the same test and evidence validator as the QEMU runner. After building a DFG-capable ARM11 E32 image, use the configured firmware's `XDG_*` and `DISPLAY` settings:

```sh
python3 Tools/Symbian/eka2l1/run-jit-tests.py \
  --image "$SYMBIAN_WEBKIT_WORK/build/e32/e32/jsc.exe" \
  --icu-data "$ICU_DIR/install/share/icu/icudt70l.dat" \
  --emulator "$EKA2L1" \
  --reports "$SYMBIAN_WEBKIT_WORK/runs" \
  --timeout 300
```

By default the runner uses one image for all three profiles. `--modes llint`, `--modes baseline`, or `--modes dfg` can isolate failures; a single-profile run does not establish cross-tier equivalence. The runner installs the image as `C:\sys\bin\jsc_arm11.exe` and takes both existing emulator locks. Each profile has a fresh guest log, host log and JSON result, with input hashes in the matrix report. A normal host exit and `jsc exiting 0` are required independently of test markers.

The test records the execution tier observed within guest frames, per-function DFG compilation counts, execution of each DFG kernel, OSR entry during a first call, and OSR exit with live values recovered. The runner compares semantic results across profiles. It rejects missing evidence, stale results, panic, timeout, and nonzero host exit. A C_LOOP-only release cannot pass as Baseline or DFG.

`python3 Tools/Symbian/tests/test-arm11-eka-runner.py -v` runs ten host-only regressions for these runner failure cases, guest exit-code reporting, and single-profile reporting. The fixtures follow the shared suite's case count. They do not run JavaScriptCore and are not tier validation evidence.
