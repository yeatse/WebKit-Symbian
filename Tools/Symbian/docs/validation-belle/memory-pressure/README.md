# Native memory pressure: guest accounting and notifications

The original EKA host reports constant free RAM even after a real 64 MiB RChunk
commit. The bounded native baseline confirms this limitation; it does **not**
establish working memory notifications or browser cache reclamation.

`baseline/result.json` records host `71cbce0a…` and probe `13eccaba…`, normal host
exit 0 and guest `EXIT error=0`. The actual log reports 268,435,456 free bytes
before reservation, after commitment and after release. The committed page
patterns and observer cancellation pass; accounting is explicitly UNAVAILABLE.
No notification is required when a real threshold crossing has not been measured.
The original runner and complete emulator log are retained. `raw-index.json`
records original-byte SHA-256 values; gzip is lossless.

Patch [0021](../../../patches/eka2l1/0021-Account-guest-RAM-and-notify-memory-thresholds.patch)
retains EKA's existing per-version total RAM and allocation behavior. HAL free
RAM subtracts committed bytes of each unique non-ROM kernel chunk once; a shared
mapping is not a second allocation, and a reservation is not a commitment.
The count saturates at zero, without adding an allocation limit. This is the
emulated writable guest-memory accounting model, not host RSS, host free RAM,
or a claim to model every physical overhead of real hardware.

`chain-0021.json` records a clean ordered replay of root patches 0001,
0005–0018 and 0020–0021 against EKA revision `5bbc0435…`. All eight owned
production source files match that replay byte for byte. Patch 0019 remains a
separate patch inside the pinned Dynarmic submodule.

The official pinned contract is in `official-contract.json`. Symbian starts its
kernel thresholds at 0/KMaxTInt; an authorized OOM service can configure them
using `UserSvr::SetMemoryThresholds`. EKA does not implement that privileged Exec
or run the device's OOM policy service. The patch therefore declares **emulator
notification defaults of 8/24 MiB**, rather than attributing these values to Nokia
firmware. It follows the official strict-low/inclusive-good crossing and
initial/coalesced RChangeNotifier semantics. Allocation-failure OutOfMemory
events and saved-state requester restoration are outside this patch's claim.

The `host/` evidence records 9,717 production policy checks, 16 actual native
bridge checks with SDK mocks, and 46 actual EKA notifier/accounting checks with
a mocked request scheduler. The latter include cancellation, closing, kernel
teardown, dead requesters and same-address/new-UID reuse. Actual EKA kernel,
chunk, notifier and HAL TUs passed syntax compilation. The native bridge and
WTF handler passed actual ARM compilation. ARM destructor disassembly confirms
that the observer and probe timer release CBase allocations with `User::Free`.

The repaired host `046bc411…` passes both native runs, with normal guest and
host exit 0. `accounting-after/` uses the identical baseline probe `13eccaba…`:
reservation leaves free memory at 266,240,000 bytes; committing 64 MiB lowers it
to 199,131,136; release restores 266,240,000. Page patterns remain intact.

`threshold-after/` uses final probe `d13a09da…` and passes accounting, low-memory
notification, recovery, coalescing, live-page preservation and cancellation.
Normal 4 MiB RChunk commitments reach 248 MiB live data and 6,189,056 free bytes;
the real notifier reports `0xa0` (FreeMemory | LowMemory). Decommitting 24 MiB
recovers 31,354,880 free bytes and reports `0x20`. The remaining 224 MiB of live
page patterns survive. Coalescing returns `0xa0`, and the callback count remains
three after monitor cancellation and another actual low/recovery crossing.
No threshold setter or synthetic notification is used. Complete guest/emulator
logs, original results and runtime drivers are retained for both runs.

`threshold-build.json` records the bounded real-RChunk probe and source hashes.
WTF source selection and the independently reviewed embedding callback are
integrated for the next browser candidate. **The browser handler and cache
relief have not yet been accepted in a real guest.** Native live-page survival
does not replace the separate browser test requiring actual pressure, bounded
relief and preservation of at least 80 MiB of rooted JS cells plus buffers.

Reproduction commands and limits are in the
[test README](../../../tests/memory-pressure/README.md). No test invokes a
synthetic JS pressure event or changes kernel thresholds to manufacture success.
