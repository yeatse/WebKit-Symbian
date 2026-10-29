# ARM traditional inline-cache literal boundary

The production `JITByIdGenerator::generateFastCommon` method reserves a 64-byte
property inline cache. Replacing that region must not overwrite literals used
by earlier instructions. Previously `padBeforePatch()` reserved only a
12-byte patchable jump; emitting the remaining NOPs could flush an older pool
inside the 64-byte region. Repatching then changed an earlier LDR's constant
into ARM NOP `0xe1a00000`.

`reproduce.py` extracts the actual old and current method bodies, changing only
the test holder's class name. Both use the current real `CCallHelpers`,
`MacroAssemblerARM`, `ARMAssembler` and constant-pool code. The holder records
real buffer offsets when labels are assigned. The probe runs on QEMU ARM1136,
scans 1,100 instruction positions for each of 1, 2 and 8 outstanding literals,
checks the jump's fifteen padding words, overwrites the actual `done-start`
region, and checks every earlier LDR's literal remains intact. The final label
must not append a pool: `StructureStubInfo::inlineCodeSize()` includes that
label's offset, so such a pool would also be overwritten.

Use a previously built ARMv6 Linux JSC build and its GCC/QEMU environment:

```sh
. /path/to/qemu/env.sh
python3 Tools/Symbian/tests/arm11-inline-cache/reproduce.py \
    --checkout "$PWD" --qemu-build /path/to/qemu/build \
    --sysroot "$ARM11_TOOLS_ROOT" --qemu "$QEMU_ARM" \
    --work /path/to/ic-regression
```

This compiles only the tiny probe, links existing libraries, and never runs
Ninja or changes the supplied build. Original source comes from fixed commit
`17cf20e6748e87a1749f11b1bf018cf2304eb392`; the incomplete body-only fix comes
from `3d20f6375cb`. Each guest run has a 30-second timeout. The script requires
demonstrated corruption and exit 1 for both earlier versions, then a clean
current run with exit 0; a crash or missing result is not an expected negative.
Source/method/log hashes are written to `result.json`.

Recorded evidence is in
[`validation-arm11/inline-cache-pool`](../../docs/validation-arm11/inline-cache-pool/README.md).
This tests real ARM code emission and repatching data, not execution of the
generated inline cache or the complete browser workload. The production
Symbian TU is checked separately with its actual SDK build flags.
