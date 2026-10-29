# Belle storage and BroadcastChannel acceptance

This fixture exercises real DOM storage, cross-frame BroadcastChannel delivery,
and document-to-DedicatedWorker transport. It is independent of native SQLite
probes: a passing SQLite implementation does not prove these page APIs work.

Serve **two origins**, retaining the same browser data directory across **three
separate browser/emulator launches**. `localhost:8780` and `localhost:8781` are
different origins. No CORS header is added. On a phone, use a reachable host name
or IP with `--bind 0.0.0.0 --host HOST`, and use that exact host in all three URLs.

```sh
python3 Tools/Symbian/tests/belle-storage/server.py --reports "$WORK/storage-reports"
```

In another terminal, with the existing EKA environment and the new browser image,
the strict runner holds both guest locks across three launches, rejects stale or
incomplete reports, checks every console result, and records image/fixture hashes:

```sh
python3 Tools/Symbian/tests/belle-storage/run.py "$BROWSER" \
  --reports "$WORK/storage-reports" --output "$WORK/belle-storage-validation"
```

The equivalent individual commands, useful for diagnosis, are:

```sh
TOKEN="storage-$(date -u +%Y%m%dT%H%M%SZ)"
WAIT=50 EKA2L1_EXPECT_PASS="BELLE_STORAGE_PASS write $TOKEN 16/16" \
  Tools/Symbian/eka2l1/run-browser.sh "$BROWSER" \
  "http://localhost:8780/index.html?run=$TOKEN&phase=write"
WAIT=15 EKA2L1_EXPECT_PASS="BELLE_STORAGE_PASS read $TOKEN 3/3" \
  Tools/Symbian/eka2l1/run-browser.sh "$BROWSER" \
  "http://localhost:8780/index.html?run=$TOKEN&phase=read"
WAIT=15 EKA2L1_EXPECT_PASS="BELLE_STORAGE_PASS erased $TOKEN 1/1" \
  Tools/Symbian/eka2l1/run-browser.sh "$BROWSER" \
  "http://localhost:8780/index.html?run=$TOKEN&phase=erased"
python3 Tools/Symbian/tests/belle-storage/verify-reports.py "$WORK/storage-reports" "$TOKEN"
```

Run the commands sequentially and stop on any failure. The runtime owner must
hold the existing exclusive EKA/XDG ownership. Do not run another guest beside
these launches. `run-browser.sh` requires normal host shutdown and a fresh log
marker, and rejects guest panics/access violations. This page cannot itself
certify host shutdown; preserve all three driver results and raw guest logs.
Also preserve browser/host/ICU/SDK identities with the reports. Never mark a
missing report or fewer outcomes as PASS. Reports reject overwriting the same
token and phase; use a new token for a new attempt.

The write phase has a 45-second page deadline and 16 named cases:

- UTF-16 localStorage operations, key enumeration, atomic 5 MiB quota rejection.
- sessionStorage operations and same-frame-tree sharing.
- Same-origin localStorage sharing; different-port local/session isolation.
- Real storage event payload and no notification back to the writer.
- BroadcastChannel structured clone, no self echo, and DataCloneError.
- Same-origin iframe roundtrip, different-origin bidirectional non-delivery,
  and `close()` suppressing queued/subsequent delivery and rejecting new posts.
- Real ArrayBuffer transfer to and from a DedicatedWorker, including sender
  detachment; localStorage/sessionStorage remain absent in that worker.
- Document/worker BroadcastChannel roundtrip and worker channel close behavior.
- A token-specific persistent value staged for the following launch.

The read phase verifies the value survived a full restart, sessionStorage did
not, and removes the persistent value. The third launch verifies deletion was
persisted. Only keys beginning `belle-storage:TOKEN:` are touched; existing user
storage is never cleared. Use a test profile with free quota, since unrelated
keys on the same origin can legitimately exhaust storage. Temporary keys are
removed on successful cases; a failed run may retain only its token's keys.

The quota case is specific to this port's configured 5 MiB localStorage limit.
It attempts a 6 Mi-character ASCII string (at least 6 MiB even with WebCore's
one-byte Latin-1 storage), then checks the old value remains. It does not infer
available phone RAM or disk space from this quota.
The bounded non-delivery checks wait after the remote peer acknowledges being
ready. Worker coverage involves executing code on a real worker and receiving
its messages, not merely checking that `Worker` exists.

These cases do not establish crash/power-loss durability, IndexedDB, SharedWorker,
ServiceWorker, CacheStorage, private browsing, storage partitioning across
different top-level sites, or cross-process BroadcastChannel support. Session
storage is expected to disappear because these are new page sessions, not a
browser session-restore operation.

For fixture maintenance, `python3 test-contract.py` checks rejection of stale,
missing, duplicate, failed, or overdue outcomes. With Python Playwright and a
local Chromium installed, `python3 check-host.py --reports REPORTS` runs the
same three phases against the running server, closing Chromium fully between
phases while retaining a temporary profile. It blanks test tabs before shutdown
so Chromium cannot silently restore the write page. This checks the fixture;
it does not establish support in the Symbian image.

The server, strict runner, host check, and report checker also accept
`--suite backends` for the separate [IndexedDB/Web Locks fixture](../belle-backends/README.md).
Without that option, this suite still runs exactly 16/3/1 outcomes on ports
8780/8781.
