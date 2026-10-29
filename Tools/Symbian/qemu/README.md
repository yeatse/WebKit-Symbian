# ARM11 execution checks

This route runs a Linux ARMv6, ARM-state, VFPv2/softfp JSC image on QEMU's `arm1136`. It checks the generated CPU code. Symbian executable memory, instruction-cache flushing, native threads and E32 imports still require the separate EKA/device route.

```sh
export SYMBIAN_WEBKIT_WORK=/absolute/path/to/arm11-work
Tools/Symbian/qemu/prepare-tools.sh
. "$SYMBIAN_WEBKIT_WORK/qemu/env.sh"
export ICU_DIR=/absolute/path/to/existing-icu-70.1-work
Tools/Symbian/qemu/build.sh --dependencies-only
CONFIGURE_ONLY=1 Tools/Symbian/qemu/build.sh
```

The preparation script downloads Debian packages and extracts them under the selected work directory. It does not install packages or write to the Symbian SDK. Package SHA256 values are retained in `qemu/apt/packages.sha256`. The compiler sysroot is `ARM11_TOOLS_ROOT`; QEMU's loader root is `ARM11_LINUX_SYSROOT`, its `usr/arm-linux-gnueabi` child. The generated environment supplies both paths and the host libraries needed by the relocated cross tools.

The ICU source and host tools are read from `ICU_DIR`; the Linux ARM libraries and objects are written under this work directory's `qemu/icu*`. `ICU_SOURCE_DIR` and `ICU_HOST_DIR` can override the source and host-tool locations individually. The default JSC build uses the shared `SYMBIAN_JSC_MODE=dfg` settings: ARM LLInt, Baseline JIT and DFG enabled, C_LOOP and FTL disabled. `CONFIGURE_ONLY=1` does not run Ninja. Build coordination must keep one owner for each build directory.

After building the DFG-capable image:

```sh
python3 Tools/Symbian/qemu/run-jit-tests.py "$SYMBIAN_WEBKIT_WORK/qemu/build/bin/jsc"
```

The runner first checks the ELF32 ARMv6/VFPv2 soft-float attributes. It executes three profiles with the same binary: JIT disabled, Baseline only, and Baseline plus DFG. `--profile` selects a subset; `--timeout` bounds each process group. Every run has fresh logs and a manifest with the JSC/test/QEMU hashes, exact commands, observed tiers and semantic results.

`tests/arm11-jit.js` checks actual frames with `$vm.llintTrue()` and `$vm.baselineJITTrue()`, and actual optimized execution with `$vm.dfgTrue()` plus per-function compilation counts. DFG must execute each tested kernel, enter a hot loop during its first invocation, and leave optimized code while preserving live integer, double and object values. Other cases cover property/call inline caches, argument adaptation, softfp/libm calls, overflow/NaN/negative zero, GC, resizable/growable buffers and exceptions. All profiles must produce identical semantic results. Enabled macros, compilation alone and a final PASS line are insufficient evidence.

The Python evidence gate is shared with `eka2l1/run-jit-tests.py`. Its host-only rejection tests are:

```sh
python3 -B Tools/Symbian/tests/test-arm11-jit-evidence.py -v
```

When a full `JSTests/stress` checkout is available, `run-stress.sh` first verifies the selected tier, then runs a deterministic sample (`MODE=llint|baseline|dfg`, `SAMPLE`, `TIMEOUT`, `JOBS`). A source archive without that test directory fails explicitly. Neither a toolchain hello program nor host-only runner tests establish working JSC JIT/DFG execution.
