# ARM11 validation evidence

`arm11-exec-probe.log` and `exec-probe-result.json` record the standalone executable-memory probe run on 2026-10-29. All 20 checks passed; the guest and EKA2L1 both exited with status zero, without a panic or access violation.

The tested source is [the executable-memory probe](../../tests/arm11-exec-probe/README.md), which documents the SDK build and bounded emulator invocation. It compiles `Source/WTF/wtf/symbian/SymbianSystem.cpp` from this checkout directly. Use a separate `SYMBIAN_WEBKIT_WORK` directory for ARM11 results and the documented emulator locks.

The JSON records the executable and validation-emulator SHA-256 hashes. No executable, firmware, host log, or user-specific host path is included here. This evidence establishes code-chunk allocation, actual guest ARM execution and rewriting, 16 MB near branches, and data-page regressions. Baseline JIT, DFG, and physical ARM11 instruction caches require separate validation.

## EKA2L1 JSC tier validation

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

`python3 Tools/Symbian/tests/test-arm11-eka-runner.py -v` runs eight host-only regressions for these runner failure cases. These fixtures do not run JavaScriptCore and are not tier validation evidence.
