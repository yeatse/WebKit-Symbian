# Native memory bridge fault injection

Run `./run.sh` with a host C++20 compiler (`CXX` may select one). The test compiles
`Source/WTF/wtf/symbian/SymbianSystem.cpp` itself against small fake EKA2 and
hardware entropy interfaces. It covers alignment and guard bounds, preserving
already committed pages, rollback when a later page commit fails, decommit
idempotence and zeroing, the reservation limit and slot reuse, and full-output
clearing after failed or short entropy reads.

The fixture uses host `mmap` for its fake chunks. It validates the bridge's
bookkeeping and error handling, not EKA2's kernel semantics, thread APIs, true
entropy quality, or JSC's physical-memory accounting. Guest RAB/GSAB and native
runtime tests remain necessary.

`python3 run-aligned-pool.py` additionally extracts the current `AlignedPool`
class directly from `FastMalloc.cpp` and compiles it with the real bridge. It
does not keep a second implementation of the allocation algorithm. The fixture
injects failures into native reservations/page commitment and the extra-region
metadata allocator; the preprocessor routes the pool's `free` identifier to a
test wrapper for tracking metadata reclamation. Kernel functions and host
allocator failures are mocked, not the pool's bookkeeping.

This covers growth past 64 MiB (65 MiB live across nine regions when native
reservations are limited to 8 MiB), owner lookup, middle/tail region reclamation,
metadata/reserve/first-commit rollback, existing-region commit failure without
growth, size overflow, alignment, fragmented free space, zero-on-reuse, and
four concurrent allocating/freeing threads with retained data patterns. Extra
regions must be reclaimed; the first region stays reserved and empty.

The old fixed-pool failure can be reproduced from a historical source file:

```sh
git show 17cf20:Source/WTF/wtf/FastMalloc.cpp > /tmp/legacy-FastMalloc.cpp
python3 Tools/Symbian/tests/native-memory/run-aligned-pool.py \
  --legacy-source /tmp/legacy-FastMalloc.cpp
```

That separate expectation verifies exhaustion near 4,096 16-KiB blocks for the
64-MiB pool (or 512 blocks after an 8-MiB reservation downgrade), while another
independent native reservation still succeeds. Depending on the mock kernel's
base alignment, the old pool can lose one block at its edges. The host fixture
does not establish a phone's maximum address space, available physical RAM,
or the cause of a particular historical allocation failure.
