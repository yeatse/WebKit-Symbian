# Inline-cache pool corruption regression

The old production emitter reproducibly corrupts an earlier LDR's literal when
the 64-byte inline cache is replaced. At one boundary, padding=496 produces
start=1988, expected end=2052 and final label=2068; replacement changes
`0x12345678` to the ARM NOP `0xe1a00000`. Across 3,300 cases the old code has
81 corrupted literal references and 36 regions with non-NOP padding.

Reserving the entire region and its jump literal before recording the start
eliminates both: the final method has zero corruptions and zero invalid padding
across the same 3,300 cases. Six final labels flush pools after the reserved
region; those are safe and preserved as an informational count.

The test extracts the actual function bodies and uses real MacroAssembler
classes on ARM1136 QEMU. Logs are original guest bytes; `result.json` identifies
their source and method hashes. The final production Symbian TU also passed
SDK ARM syntax checking. See the [probe and reproduction command](../../../tests/arm11-inline-cache/README.md).

The normal ARM32 DFG get/put-by-id paths call this emitter; their stubs default
to `useDataIC=false`. This proves a real reachable corruption defect. It does
not alone prove that a particular media-page crash used that exact inline cache;
the unchanged browser workload must be rerun with the fix.
