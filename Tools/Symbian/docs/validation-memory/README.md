The phone's `log-1.txt` reports an intentional `MemoryExhaustion` assertion at
`LocalAllocator.cpp:150` after loading `chatgpt.com`. This means a new JSC marked
block could not be allocated. The log contains no pool counters, failed native
request/error, or allocator statistics at that point, so it cannot establish
whether that particular failure was pool capacity, fragmentation, physical
commitment, or unavailable process address space. System-wide free RAM alone
does not distinguish these possibilities.

The actual system-malloc path is `MarkedBlock::tryCreate` →
`FastMallocAlignedMemoryAllocator::tryAllocateAlignedMemory` →
`tryFastCompactAlignedMalloc` → `tryFastAlignedMalloc` → the Symbian
`AlignedPool`, requesting a 16-KiB block with 16-KiB alignment. The prior pool
had one reservation of at most 64 MiB; if the kernel rejected that size at
startup it tried 32, 16, then 8 MiB. Once that one pool filled, no further region
was tried. Ordinary SDK mimalloc's separate 64-MiB address budget is not this
block allocation's direct path. JIT executable memory is also separate.

The released `17cf20e6748e87a1749f11b1bf018cf2304eb392` implementation was compiled
directly in the [host fixture](../../tests/native-memory/README.md). It exhausted
after 4,096 16-KiB blocks at 64 MiB, or 512 blocks after an injected 8-MiB native
reservation limit. At exhaustion the failing allocation made no kernel commit
attempt, while an independent native reservation/commit still succeeded.
Alternating retained pages also reproduced rejection with half the pool free
but no contiguous 16-KiB run. This proves a software capacity/fragmentation
boundary; it does not retroactively identify the phone's exact failure.

The new pool searches existing regions first and adds a separate native region
when no aligned run fits. Each region still starts at no more than 64 MiB and
falls back toward 8 MiB on reservation failure; the successful smaller size is
remembered, and a fully failed attempt reduces subsequent requests to the
minimum. No permanent failed-reservation latch prevents a later retry.
Metadata allocation, reservation, and first commitment must all succeed before
a region is published. A commitment failure does not create additional regions.
Free locates the owning region under the pool lock; an extra region's final
allocation releases its complete reservation and metadata. The first region
remains reserved with free pages decommitted. Ordinary malloc is not used as an
aligned-memory fallback because commit/decommit require a registered native
reservation. The existing 64-MiB maximum for one individual allocation remains.

`host-summary.json` and the three `host-*.log` files record the results. The
production implementation retained 65 MiB across nine 8-MiB regions under the
mock native-size limit, preserving data and reclaiming all extra regions on
free. Tests also passed for metadata/reserve/commit rollback, existing-region
commit failure without growth, overflow/alignment rejection, fragmentation,
zero-on-reuse, middle/tail removal, and four concurrent allocation/free threads.
The actual ARM compiler accepted `FastMalloc.cpp` and `SymbianSystem.cpp`.

The initial growth version in `host-summary.json` is deliberately retained with
its original source hash. Subsequent [native pressure runs](native-pressure/README.md)
found a separate compiler-visible stack defect: aggregate assignment of the
first region created a 34-KiB temporary on the allocation path, overflowing an
ordinary 8-KiB pthread stack. The final source resets only the two fields changed
before the failed first commitment. Its production allocation frame is 40 bytes
instead of 34,880 bytes. The final source hashes, repeated host fault suite,
compiler stack-usage evidence and native 7/7 pass are recorded under
`native-pressure/growth-final/`; both the old single-pool negative run and the
intermediate stack failure remain archived. The native test now retains and
checks 80 MiB, then passes four threads without increasing their stacks.

The separate [JSC integration run](jsc-cell-pressure/README.md) retains over
72 MiB of actual live object-space bytes after full GC, verifies both complete
object graphs, and observes them fall below 87 KiB after root release and GC.
That first successful shell predates the stack fix and is explicitly identified
as such; its main-thread result does not validate the failed threaded version.
Only candidates built from the final stack-reset source should be considered
for delivery, together with the independent JIT and browser checks.

Failure diagnostics use a fixed stack buffer and direct stderr writes, without
WTF strings or buffered FILE output. `reason`, `request`, and `alignment` locate
the failing stage. `pool-reserved`/`pool-used` describe this aligned pool;
`pool-largest-free` is the largest raw contiguous run, before applying the
requested alignment. `chunk-error` is the native return value when applicable;
zero on a pool-level message does not mean the allocation succeeded.
`chunks`, `chunk-reserved`, and `chunk-committed` describe the WTF bridge's
registered reservations and logical page accounting, excluding SDK mimalloc,
other native chunks, alignment padding, and unused physically committed code
chunk space. `system-free` is HAL's current free RAM in bytes, or -1 if unknown.
These counters must not be interpreted as total process memory usage.

The host tests execute the real pool algorithm and bridge with mocked kernel,
HAL, entropy, and metadata-failure interfaces. They do not establish a real
phone's RChunk/address-space ceiling or prove `chatgpt.com` now fits in memory.
Native guest pressure passes are recorded above. Another physical-phone run
remains necessary to identify the original failure and validate that workload.
