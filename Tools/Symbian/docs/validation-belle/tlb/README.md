# Invalid TLB entries during EndTask

The old emulator crashes in generated x86-64 code while handling a guest null
read during EndTask. The preserved GDB excerpt shows guest PC `0x855ea7e2`
(Thumb, ROM `uiklaf.dll + 0x7e2`), guest address `4`, a TLB tag of zero, and a
null host base. The generated code compares the shifted tag to guest page zero
and directly executes `mov (%r9,%r12),%eax` with `r9=0` and `r12=4`.

Patch 0019 uses an unaligned invalid tag for empty/denied entries and preserves
that distinction during generated comparisons. It also rejects null host bases.
`Flush` and `MakeDirty` restore the same invalid state. Valid page-zero mappings
are still supported. Denied permissions previously also allowed page-zero
access to a nonzero page occupying the same TLB slot; the regression covers this
independently of the host crash.

The actual Dynarmic JIT A/B has 33 cases per binary:

| Control | Result |
| --- | --- |
| Original emitter and TLB header | 15 host SIGSEGVs, 4 incorrect direct hits, 14 passing controls |
| Corrected emitter and TLB header | 33/33 pass; denied/unmapped accesses reach callbacks |

`result.json` preserves every outcome. `build-manifest.json` records the actual
compile/link commands and input/output hashes. Only two memory-emitter objects
were replaced in private archive copies; the shared build archive and emulator
were not changed by the test. The final production Dynarmic diff has three
files, and 0019 applies cleanly to submodule revision `855e885288fc21a616ed07ee4437f34abe7c3922`;
the applied files were compared byte-for-byte with the tested source.

The two x86-64 emitter translation units compiled successfully. The ARM64-host
emitter passed a C++ syntax check using its actual Oaknut interfaces with the
ARM64 MCL structure selection; this is not an ARM64 runtime claim. Exploratory
A64-guest execution was excluded: this fork's existing A64 interface does not
initialize the TLB register from `conf.tlb_entries`. This patch does not extend
A64-guest support and Symbian uses the A32 interface.

Reproduction is documented in [the test directory](../../../tests/eka-tlb/README.md).
The real EndTask retry is pending. The fix routes the original guest null access
to EKA's existing MMU/exception callback; it does not suppress or resolve the ROM
error. No success is claimed for application shutdown from these host tests.
