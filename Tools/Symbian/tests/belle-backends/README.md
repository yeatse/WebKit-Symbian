# Existing IndexedDB and Web Locks backend acceptance

This exercises the providers already connected to the Symbian Page. It does not
enable another compile option or replace them. The shared transport and strict
runner live in `../belle-storage/`; `--suite backends` selects this page and its
separate outcome contract. The original storage suite remains the default.

```sh
python3 Tools/Symbian/tests/belle-storage/server.py --suite backends \
  --reports "$WORK/backend-reports"
```

With the sole EKA/XDG runtime owner, in another terminal:

```sh
python3 Tools/Symbian/tests/belle-storage/run.py "$BROWSER" --suite backends \
  --reports "$WORK/backend-reports" --output "$WORK/backend-validation"
```

The server uses `http://localhost:8784` and `http://localhost:8785`. Web Locks
requires a secure context; localhost is potentially trustworthy. Do not replace
it with a plain HTTP LAN address for phone testing and interpret the resulting
missing API as a backend defect. Such testing needs a properly trusted HTTPS
fixture. The port's backend still requires its own runtime verification.

Three separate launches preserve the same database directory and a random token:

| Phase | Required outcomes | Purpose |
| --- | --- | --- |
| write | 14/14 | Transaction/index/cursor behavior, rollback, worker/origin isolation, real lock scheduling and termination |
| read | 2/2 | Both page and worker commits survive restart; delete the database |
| erased | 1/1 | A second restart confirms deletion |

The write phase checks schema creation and committed readback, unique-index
lookup, cursor order, explicit abort rollback, uniqueness-error transaction
rollback, connection close/reopen, versionchange notification and a blocked
version-2 upgrade that resumes only when the old connection closes. A real
DedicatedWorker reads the page's row and commits another row that the page reads.
A different-port iframe cannot read or overwrite the database.

Web Locks checks exclusive ordering, two simultaneous shared holders blocking an
exclusive waiter, `ifAvailable`, cancellation of a queued request with
`AbortController`, a worker holding a lock until `terminate()` releases it, and
independent lock namespaces for different origins. It never treats API presence
or a promise merely resolving as proof of all those semantics.

The page deadline is 55 seconds. The strict runner observes 60/15/15 seconds and
uses the existing bounded graceful-shutdown/process-tree cleanup. It preserves
all phase logs, rejects stale/missing/duplicate outcomes, and records fixture and
image hashes. Do not use the same emulator data concurrently. Only the random
`belle-backends:TOKEN` database and lock names are touched.

To check the fixture itself in host Chromium with Python Playwright, while the
server is running:

```sh
python3 Tools/Symbian/tests/belle-storage/check-host.py --suite backends \
  --reports "$WORK/backend-reports"
```

This does not establish disk-full/quota handling, crash/power-loss durability,
Blob persistence, cross-process locking, private browsing, browser session
restoration, SharedWorker, or ServiceWorker. The existing IndexedDB server's
quota callback still grants requests unconditionally; successful small-database
tests do not fix that separate limitation.
