# Storage and BroadcastChannel on Symbian

Candidate 2 (`91cf3692692f…`) passes all **20 outcomes over three independent
browser/emulator launches** on EKA host `12c275e7f7b3…`: write 16/16, restart/read
and delete 3/3, second restart confirming deletion 1/1. Every driver and host
exited normally; no guest panic or access violation was observed. The
[result manifest](candidate2/result.json) associates exact image and fixture
hashes with each report and original guest-log hash. Gzip files preserve the
original guest bytes. [Build identities](../candidate2-identities.json) include
the source snapshot, SDK runtime and relevant build artifacts; no binaries are
vendored here.

The page proves localStorage string operations, quota rejection without losing
the old value, same/different-origin isolation, storage events, sessionStorage,
BroadcastChannel structured cloning, same-origin iframe transport,
different-origin non-delivery, queued/closed-channel behavior, real
DedicatedWorker ArrayBuffer transfer and BC roundtrip, and persistence/deletion
across full restarts. It does not prove power-loss durability or storage quotas
for IndexedDB. See the [fixture and strict runner](../../../tests/belle-storage/README.md).

Candidate 1 (`7b841e547878…`) is retained as a **real guest failure**, with 10 PASS
outcomes and three failures before the required 16-outcome phase could finish.
localStorage and document/iframe BC already worked, but `sessionStorage` was
hidden by its separate disabled WebCore setting. The worker's native thread
requested only 128 KiB. `RThread::StackInfo` excludes thread startup metadata,
so the effective stack was smaller than JSC's 128 KiB soft reserve: the first
script immediately threw `RangeError`. The fixes explicitly enable
`sessionStorage` and allocate 512 KiB only for `ThreadType::JavaScript`; ordinary
WTF threads stay at 128 KiB, and SDK default pthread stacks are unchanged.
The selected host thread messages retain the before/after 0x20000/0x80000 evidence.
JSC reserved zones and stack-bound checks were not reduced or bypassed.

The first candidate-2 attempt is stored separately under
`candidate2-display-unavailable`: host Xvfb display `:95` was unavailable, so
the screenshot step failed before any page result. This is a host setup failure,
not a second guest capability failure. The successful retry used the same image
and host hashes, with unchanged page expectations.
