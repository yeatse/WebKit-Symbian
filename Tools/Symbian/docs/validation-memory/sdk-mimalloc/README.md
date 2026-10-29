# SDK mimalloc: fixed-budget 32-bit comparison

All three real ARM E32 probes passed in separate EKA processes with normal
guest/host exit, complete fresh logs and no guest panic. The installed SDK
remained unchanged during the comparison. This is independent of WebKit's
16 KiB-aligned JSC pool: that pool caused the separate `LocalAllocator` failure
path and does not directly call mimalloc for those blocks.

| Runtime | First/reused 256 KiB objects | Live requested bytes | Reservation after 4,096 small objects |
|---|---:|---:|---:|
| SDK 0.3.0 stock | 63 / 63 | 16,515,072 (15.75 MiB) | 50,376,704 |
| Rebuilt A, original configuration | 63 / 63 | 16,515,072 (15.75 MiB) | 50,376,704 |
| Rebuilt B, 32-bit page map | 218 / 218 | 57,147,392 (54.5 MiB) | 16,945,152 |

The native reservation budget stayed **67,108,864 bytes** in every snapshot.
Stock and A have identical per-phase measurements. B's final pressure phase
has 54 chunks, 66,949,120 reserved bytes and 64,520,192 committed bytes; after
free/collect it retains three arenas plus the page map (50,565,120 reserved)
but only 229,376 committed bytes. Reservation and committed RAM are different
quantities. The final 72 MiB request correctly fails without increasing either
mode's reservation. Expected ENOMEM callbacks during pressure are recorded;
they are not content/alignment test failures.

The shipped configuration uses 32 MiB arena alignment for its fast free
metadata lookup, creating about 32 MiB of slack alongside a 16 MiB arena.
Upstream `MI_FREE_USE_PAGEMAP=1` selects a flat map and 32 KiB alignment on this
32-bit build. It retains the same 64 MiB native limit. In-page metadata costs
more space for some object sizes, so this is not a claim that all workloads
have the same improvement or that physical free RAM determines the limit.

`runtime-summary.json` verifies the original guest-log hashes and contains
every phase. Each mode directory preserves the guest log, compressed full
host log, runner result and available compiler/configuration records.
`build-results.json` is the earlier build-only snapshot; the separate runtime
summary records the later execution. `page-bridge-identical.json` proves the
native RChunk bridge object is identical in stock/A/B.

The production script rebuilds from fixed SDK and mimalloc revisions with the
same `-Os` configuration as A/B. Its result is byte-identical to the B archive
that ran above:

```
base:   a3311f88e8fb328e0db5fdadffa3734cf9bd4c171ed9b3a4bf057eca2c83612c
output: 726c69c4798dab91f6e6957ef340107754bde4901127732571338afbe2007e26
```

`production-verification.json` records all 103 member hashes and commands.
Only the 20 mimalloc core objects and SDK primitive change; 82 members,
including the native budget implementation, retain their original bytes.
The common symbol type/binding/visibility and data/TLS sizes, full external
strong/weak dependencies and ARM attributes are preserved, except for the
optimization goal. The only definition-set changes are removal of internal
`__mi_page_map` and addition of `_mi_page_map`/`_mi_page_map_max_address`.
All 241 public `mi_*` definitions remain. This is a symbol/ABI check, not a
proof of arbitrary C signatures or internal metadata layouts.

`reproducibility.json` covers a second build in another work directory and
two checked installations into an isolated SDK copy. No production SDK was
overwritten for those checks. The real probe source/CMake and the archive
rejection tests are in `tests/sdk-runtime`; the source patch and fixed hashes
are in `patches/sdk-0.3.0`.

After those checks, the same script explicitly installed `726c69c4…` into the
shared SDK's default ARMv6 runtime and repeated the install successfully.
`production-verification.json` is the final installation record. The original
archive remains in the production work directory's `base/` backup. No other
SDK library was replaced.

The first ten host tests did not cover work directories inside a Git checkout.
Review reproduced `git apply` returning zero while skipping the patch there.
The corrected script creates an explicit private Git root, removes inherited
Git worktree/index/configuration environment, and checks the patched header's
fixed SHA256 immediately. Three additional tests cover nested Git, polluted
outer Git settings and unexpected post-patch bytes. `nested-ci-regression.json`
and `nested-production-verification.json` record a complete rebuild from the
unmodified release archive with both the SDK input and work directory inside
the real WebKit checkout. Its output is still `726c69c4…`; the source SDK input
stays `a3311f88…`. `host-checks-nested.log` records all 13 passing cases. This
fix does not change the installed runtime or its guest A/B evidence.

This evidence establishes single-thread allocation geometry, data/alignment
integrity, reuse and budget enforcement on the named EKA fixture. It does not
establish phone-kernel behavior, throughput, or all concurrent allocations.
The final browser/JSC relink and their full runtime regressions remain separate
acceptance gates after the checked runtime is installed.
