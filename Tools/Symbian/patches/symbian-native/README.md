# E32 converter patches

Apply these patches in numeric order to the SDK source selected by
`Tools/Symbian/scripts/build-symbian-native.sh`. Keep the input ELF's ARM
attributes: they are part of the target contract, not optional data to remove
when conversion fails.

`0003` accepts an explicit ARMv6 (`Tag_CPU_arch=6`), VFPv2
(`Tag_FP_arch=2`) ELF using base AAPCS (`Tag_ABI_VFP_args=0` or absent).
Thumb-1 is supported. It continues to reject the ELF hard-float flag, nonzero
VFP argument PCS, NEON, VFPv1/VFPv3+, Thumb-2, ARMv7, and VFPv2 paired with
an unknown or ARMv5 CPU. Existing software-only profiles retain their output.

The E32 header declares VFPv2 with `0x00100000`, producing flags `0x12100028`
for an ordinary executable. These constants come from the original
[Symbian f32image.h](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/userlibandfileserver/fileserver/inc/f32image.h#L90)
and `EFpTypeVFPv2=1` in `e32const.h`. This describes hardware floating-point
instructions used, separately from the argument/result calling convention.
It does not enable hard-float interoperability. E32 inspection accepts the
VFPv2 profile only with the ARMv6 CPU identity.

In that original kernel revision, `kernel/eka/kernel/arm/ckernel.cpp`
allocates VFP context storage per thread under `__CPU_HAS_VFP` and saves or
restores it lazily in `HandleVFPOperation`, without checking the E32 FP flag.
The flag is exposed as `RLibrary::TInfoV2::iHardwareFloatingPoint` by the
loader. Correcting a missing VFP declaration is therefore a metadata fix; it
is not evidence of a demonstrated lost-register bug or a claim about every
Belle firmware implementation.

An ELF without FP attributes cannot describe dynamically emitted JIT code.
The ARM11 JIT packaging flow must explicitly declare its known VFPv2 target
and recompute the E32 header CRC; converter acceptance of missing attributes
is not evidence that the process uses no VFP instructions. Do not scan machine
code or remove ARM attributes to guess or conceal that requirement.

After applying `0001`–`0003`, build and run the host behavior tests:

```sh
cmake --build "$SYMBIAN_NATIVE_BUILD" --target symbian_native_tool symbian_e32_test -j1
"$SYMBIAN_NATIVE_BUILD/cpp/tests/symbian_e32_test"
python scripts/check_cpp_style.py cpp/symbian/e32/e32.cc \
    cpp/symbian/e32/e32.h cpp/tests/arm_attributes_test.cc
```

The style command runs from the SDK checkout with its development Python
dependencies installed. `0003` retains the previous 47 tests and adds three
behavior tests (50 total): VFPv2 conversion/inspection/code preservation,
incompatible attribute and ELF flag rejection, and E32 CPU/FP profile
validation. Later patches may add further tests.

On 2026-10-29 the host suite passed 50/50, owned C++ style had zero violations,
and clean sequential apply/reverse checks passed. A real ANGLE probe ELF
retaining ARMv6/VFPv2 attributes converted with the VFPv2 E32 flag; the previous
converter rejected the same input. Eleven separately modified copies covering
incompatible CPU, Thumb, FP, SIMD, argument PCS and ELF flags were rejected
without writing an output image. This is host format/ABI-policy evidence.
Emulator loader execution, successful GLES rendering and physical-device
compatibility each require separate execution evidence.
