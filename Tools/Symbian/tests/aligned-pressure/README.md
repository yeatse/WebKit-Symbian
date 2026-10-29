# Native aligned-allocation pressure probe

This finite E32 program compiles the current production `AlignedPool` class
extracted from `FastMalloc.cpp`, plus the real `SymbianSystem.cpp`. It does not
copy the allocator algorithm, mock `RChunk`, link a JS VM or reserve a JIT pool.
The generated `source-hashes.json` and guest `SOURCE` line identify the source
versions. Extraction matches the existing host fault-injection fixture.

It commits and retains 80 MiB in 16 KiB aligned blocks, checks every byte for
initial zeroing and distinct retained patterns, frees alternating blocks and
checks zero-on-recommit plus preservation of neighbors. It then retains
alternating 4 KiB pages from a 64 MiB allocation set and requires another 16 KiB
aligned allocation without corrupting live pages. Four pthreads exercise
allocation, writing, checking and release. The old single pool is expected to
fail the 80 MiB phase; do not relabel that run as a pass.

Real kernel commit failures cannot be injected by this probe. The production
allocator host fixture in `../native-memory/run-aligned-pool.py` separately
covers reserve/metadata/partial-commit failures, rollback, reduced native region
limits, extra-region reclamation and concurrent ownership. Its kernel mocks do
not establish physical-device behavior.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/patched/symbian-native
export PROBE_BUILD=/path/to/aligned-pressure-build
Tools/Symbian/tests/aligned-pressure/build.sh
```

For a negative control, save the original `FastMalloc.cpp` with
`git show 17cf20:Source/WTF/wtf/FastMalloc.cpp > /tmp/old-FastMalloc.cpp`, then
build into a separate directory with `PROBE_POOL_SOURCE=/tmp/old-FastMalloc.cpp`.
This changes only the extracted pool; the bridge, compiler and test remain the
same. Preserve its nonzero exit and failed allocation count as negative
evidence. It is not a reconstruction of the entire historical browser image.

The image uses UID `0xe00008f6`, ARMv6 software float, a 512 KiB stack, and a
64 MiB maximum ordinary process heap. The tested aligned pages live in separate
data chunks. This program therefore does not increase mimalloc's independent
address-reservation budget. It logs to `C:\Data\WebKit\aligned-pressure.log`
and needs no arguments. With the isolated EKA environment selected, its sole
owner can run:

```sh
python3 Tools/Symbian/eka2l1/run-native-probe.py \
    --probe aligned-pressure --image "$PRESSURE_IMAGE" \
    --image-sha256 "$PRESSURE_SHA256" --emulator "$EKA2L1" \
    --emulator-sha256 "$EKA2L1_SHA256" --reports "$REPORTS" --timeout 180
```

Require all checks, `retainedBytes=83886080`, normal guest/host exit zero, no
panic and the final PASS marker. A timeout or forced host close is a failure.
Free-system-RAM readings are context, not substitutes for allocation checks.

For the separate JSC integration test, run `../jsc-cell-pressure.js` through the
JSC runner with `TIMEOUT=180` and expected line `PASS: jsc-cell-pressure`. It
retains a linked graph of small objects across full collections until measured
live object-space bytes (`heapSize - extraMemorySize`) reach 72 MiB, validates
the entire graph, releases it, and repeats allocation/collection. It uses no
ArrayBuffer payload or giant root array. This checks ordinary JSCell allocation
and GC reuse; it does not by itself prove which JIT tier executed. Run the same
script with JIT disabled as a separate control if code-pool effects need to be
distinguished. A physical phone result requires executing these exact images on
that phone, not only EKA or the host fixture.
