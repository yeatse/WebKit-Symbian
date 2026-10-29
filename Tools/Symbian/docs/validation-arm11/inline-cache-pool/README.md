# Inline-cache pool corruption regression

The original fixed-64-byte test was incomplete. Its `before.log`, `after.log`
and `result.json` are preserved unchanged, but the earlier assertion that six
end-label pool flushes were safe is withdrawn. Production replacement uses
`StructureStubInfo::inlineCodeSize()`, which is **done minus start**, not the
requested 64 bytes. A pool emitted by the final label is inside that range.

The corrected `actual-range/` test overwrites the true range and separately
requires its size to remain exactly 64 bytes. Each version scans 3,300 cases:

| Version | Corrupted earlier literal references | Invalid padding | Enlarged regions |
| --- | ---: | ---: | ---: |
| Original `17cf20` | 165 | 36 | 45 |
| Body-only reservation `3d20f6375cb` | 22 | 0 | 6 |
| Full reservation including final label | 0 | 0 | 0 |

Both earlier versions corrupt `0x12345678` into NOP `0xe1a00000` at padding=491,
start=1968, requested end=2032 and actual end=2048. The final fix reserves the
64-byte body, the end label's 4-byte instruction guard, the jump's 4-byte
literal, and the end label's 8-byte constant guard before recording start. Its
size assertion runs after the final label has been emitted.

The test extracts the actual function bodies and uses real MacroAssembler
classes on ARM1136 QEMU. Logs are original guest bytes; each `result.json`
identifies its source and method hashes. The final production Symbian TU passed
SDK ARM syntax checking. See the [probe and reproduction command](../../../tests/arm11-inline-cache/README.md).

The normal ARM32 DFG get/put-by-id paths call this emitter; their stubs default
to `useDataIC=false`. This proves a real reachable corruption defect. It does
not alone prove that a particular media-page crash used that exact inline cache;
the unchanged browser workload must be rerun with the fix.
