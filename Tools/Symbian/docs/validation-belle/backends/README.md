# Existing IndexedDB and Web Locks backends on Symbian

Candidate 2 (`91cf3692692f…`) passes **17 outcomes over three independent
browser/emulator launches** on EKA host `12c275e7f7b3…`: write 14/14, restart/read
and delete 2/2, second restart confirming deletion 1/1. All driver/host exits
were zero, with no guest panic or access violation. The
[result manifest](candidate2/result.json) records exact fixture/image/log hashes;
the phase gzip files preserve original guest output bytes. Source and SDK
identities are in [the shared candidate record](../candidate2-identities.json).

This exercises committed transactions, an index and ordered cursor, explicit
abort, unique-index error rollback, connection close/reopen, versionchange and
blocked upgrade coordination, real DedicatedWorker reads/writes, origin
isolation, exclusive/shared locks, `ifAvailable`, cancellation, worker termination
releasing its lock, and separate-origin lock namespaces. See the
[fixture and reproduction](../../../tests/belle-backends/README.md).

No new IDB or Web Locks provider was needed for these cases: they already had
real Legacy/WebCore backends. The independent sessionStorage gate and worker
stack fixes are documented in [the storage result](../storage/README.md).
This evidence does **not** establish an IndexedDB quota policy, Blob atomicity,
disk-full handling, crash/power-loss recovery, or cross-process operation.
The existing quota callback still grants positive requests unconditionally;
that limitation is being handled separately from this verified baseline.

The first candidate is retained under `candidate1`: six IDB checks passed before
the worker's initial script raised `RangeError`. The page reported 6/7 with a
FAIL marker, short of the required 14 checks. Its outer runner was interrupted
before persisting an exit status, so that status remains unknown; no successful
shutdown is inferred. Original guest and launcher bytes and the failure report
are preserved. The later candidate fixes the worker stack without reducing the
fixture's expectations.
