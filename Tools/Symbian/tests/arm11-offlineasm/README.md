# ARM11 offlineasm execution probe

This test runs the production `ARM` offlineasm backend on QEMU's **ARM1136**
CPU model. It generates the assembly from `probe.asm`, compiles a small static
Linux ARM executable, and checks the computed results. It needs Ruby, a Linux
ARM EABI GCC, and `qemu-arm`; it does not build JavaScriptCore or require LLVM,
a target libc, Symbian firmware, or EKA2L1.

```sh
ARM_CC=arm-linux-gnueabi-gcc-14 QEMU_ARM=qemu-arm \
    Tools/Symbian/tests/arm11-offlineasm/run.sh
```

For the prepared workspace tools:

```sh
. /workspace/host-env.sh
. /workspace/symbian-arm11-work/qemu/env.sh
ARM11_OFFLINEASM_BUILD=/workspace/symbian-arm11-work/qemu/offlineasm-probe \
    Tools/Symbian/tests/arm11-offlineasm/run.sh
```

`ARM_CC`, `QEMU_ARM`, and `RUBY` select executables. Set
`ARM11_OFFLINEASM_BUILD` to retain the generated header, extractor envelope,
and executable; otherwise they are created in a temporary directory and
removed. `ARM11_OFFLINEASM_TIMEOUT` defaults to 20 seconds. Any generation,
compilation, execution, timeout, or result mismatch fails the script.

The executable is built with `-march=armv6 -marm -mfpu=vfp
-mfloat-abi=softfp`, and QEMU always runs it with `-cpu arm1136`. The small C
harness only uses Linux `write` and `exit` system calls. The extractor envelope
selects the real `ARM` backend; the fixture has no C++ offsets or sizes, and
the generator rejects layout-dependent constants rather than inventing values.

Expected final output:

```text
PASS: in-place tzcnti including zero
PASS: constants and symbolic literals
PASS: signed unsigned and equality comparisons
PASS: VFPv2 comparisons NaN infinity and signed zero
PASS: large signed offsets and halfword base aliases
PASS: arbitrary register pairs and base aliases
PASS: arm11-offlineasm checks=0x0000210c
```

The 8,460 checks cover every single-bit input and 2,048 deterministic random
inputs for in-place trailing-zero count; large positive/negative constants;
absolute symbols and label differences; signed/unsigned/equality comparisons
with source/destination aliases; VFPv2 ordering, NaN, infinity, signed zero and
subnormal values; large positive/negative memory offsets; scaled halfword
addresses with the base also serving as the destination; and arbitrary
register pairs, including either loaded destination aliasing the address base.
Stores also check adjacent bytes or words remain unchanged.

This proves execution of the tested instruction lowering on an ARMv6 CPU
model. It does **not** establish that the full JSC LLInt, baseline JIT, DFG,
OSR, GC integration, executable allocator, or Symbian system calls work.
The wrappers preserve the C ABI registers independently of the LLInt entry
sequence, so this is not a test of LLInt frame layout or callee-save slots.
