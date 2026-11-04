# ARM11 offline assembler

Generates ARM assembly with JavaScriptCore's offline assembler and executes it
with QEMU. Requires Ruby, an ARM EABI GCC toolchain and `qemu-arm`.

```sh
ARM_CC=arm-linux-gnueabi-gcc QEMU_ARM=qemu-arm \
  Tools/Symbian/tests/arm11-offlineasm/run.sh
```

The checks cover the ARM instruction sequences used by the interpreter and JIT.
The runner exits successfully when the computed results match the fixture.
