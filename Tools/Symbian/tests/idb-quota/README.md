# IndexedDB quota and Blob transaction regression

The Symbian in-process IDB server limits committed files plus pending transaction
reservations to **32 MiB per full ClientOrigin** (top origin + client origin) and
**128 MiB across the port's IDB directory**. Persistent-write and commit admission
checks require **16 MiB of free space** on its actual drive after estimated
pending writes. This is an admission check, not a system-wide disk reservation.
Blob staging copies are created before server quota admission and can consume
temporary space first; their actual allocation is reflected in free-space
queries, and normal staging I/O failures clean up that request's temporary files.
The native volume bridge and EKA
patch 0018 are prerequisites for meaningful emulator free-space results.

The accounting cache is initialized from disk at server startup. Different
transactions/databases share their origin and global budget. Failed reservation
does not change the ledger; actual rollback/commit releases only that transaction.
Before commit, the engine checks actual SQLite pager growth and staged Blob file
sizes, including index growth missed by estimates. DELETE journals already
consume available drive space but disappear from the projected committed size;
WAL/SHM and existing Blob files remain charged. Normal I/O/SQL failure rolls back
before new Blob files are removed. This does **not** establish power-loss
or process-kill durability: copied Blob files are not fsynced.

Old over-limit data remains readable and deletable. Conservative estimates may
reject an overwrite before its final footprint is known. Clearing records does
not shrink SQLite's allocated pages; deleting the database releases those pages.
Blob references cleared successfully are released at commit, while abort keeps
the original Blob files. Administrative clearing of origins rescans once any
unrelated outstanding transactions finish, retaining their reservations meanwhile.

Host checks compile the production accounting and Blob commit coordinator:

```sh
Tools/Symbian/tests/idb-quota/run-ledger.sh
SQLITE_SOURCE=/path/to/sqlite-amalgamation-3350500 \
  Tools/Symbian/tests/idb-quota/run-blob.sh
python3 Tools/Symbian/tests/idb-quota/run-producer.py --work "$TEST_WORK/producer"
python3 Tools/Symbian/tests/idb-quota/test-contract.py
python3 Tools/Symbian/tests/idb-quota/test-server.py
python3 Tools/Symbian/tests/idb-quota/check-syntax.py \
  --compdb /path/to/existing/webcore/compile_commands.json \
  --work /path/to/fresh/syntax-output --sdk /path/to/symbian-sdk
```

`run-blob.sh` uses real SQLite with DELETE journals and real files. It covers
partial copy failure, a second SQLite connection causing COMMIT `SQLITE_BUSY`,
an existing destination, and a creator racing the exclusive copy. Only files
successfully created by this transaction are cleanup targets. The test does not
run the full WebCore backend; the page regression checks integration.

`run-producer.py` extracts and compiles the actual BlobRegistry producer and
writer bodies. With real files and injected short writes/open failure, it checks
cleanup of the current partial output and earlier staging copies from the same
request, closure before deletion, and preservation of the original File input.
The filesystem/queue/Blob seams are test adapters; the producer code is not
reimplemented. A historical source can be checked as a negative control:

```sh
git show 94181dea:Source/WebCore/platform/network/BlobRegistryImpl.cpp \
  > "$TEST_WORK/BlobRegistryImpl-before.cpp"
python3 Tools/Symbian/tests/idb-quota/run-producer.py \
  --source "$TEST_WORK/BlobRegistryImpl-before.cpp" \
  --work "$TEST_WORK/producer-before"
# Expected nonzero exit: the first short write leaves its partial staging file.
```

The production ownership chain is `SerializedScriptValue::writeBlobsToDiskForIndexedDB`
to `BlobRegistryImpl::writeBlobsToTemporaryFilesForIndexedDB`, then `IDBValue`.
Every handed-off path comes from a new `openTemporaryFile("Blob")` copy; source
File paths are inputs to the copy, never cleanup targets. Once handed off, the
SQLite transaction owns those copies through commit, abort or rejection.

To additionally run the same production ledger on 32-bit ARM1136:

```sh
. /path/to/qemu/env.sh
arm-linux-gnueabi-g++ --sysroot="$ARM11_TOOLS_ROOT" -std=c++20 -O2 \
  -march=armv6 -marm -mfpu=vfp -mfloat-abi=softfp \
  -ISource/WebCore/Modules/indexeddb/server \
  Tools/Symbian/tests/idb-quota/ledger.cpp -o "$TEST_WORK/ledger-arm"
"$QEMU_ARM" -cpu arm1136 -L "$ARM11_LINUX_SYSROOT" "$TEST_WORK/ledger-arm"
```

## Actual browser regression

Start the six-origin server and let the sole EKA owner run the fixture. Each run
uses a unique database-name prefix; no unrelated storage is cleared. The write
phase writes roughly 240 MiB over time (at most 128 MiB retained), then leaves
24 MiB for a process restart. Use a disposable test origin and sufficient disk.
Do not run this concurrently with any other emulator task.

```sh
python3 Tools/Symbian/tests/idb-quota/server.py --reports "$REPORTS"
# In the already configured EKA/XDG environment:
python3 Tools/Symbian/tests/idb-quota/run.py "$BROWSER_EXE" \
  --origin http://localhost:8790 --reports "$REPORTS" --output "$RESULTS"
```

The server binds ports 8790–8795 (override `--base-port` and runner `--origin`
together). No CORS permission is granted. Cross-origin commands use exact
`event.origin`, `event.source` and run-token checks. Three independent launches
must pass **12 write + 3 restart/read + 1 deletion/restart cases**. A successful
feature-detection check is never accepted as quota evidence. Each launcher is
bounded by a host watchdog, checks fresh guest logs and normal emulator exit,
and requires its exact per-case console lines plus an immutable JSON report.
The write deadline is 240 seconds; the other page phases have 60-second bounds.

The suite checks Blob replacement/repeated put/clear/abort, oversized Blob
rejection without damaging its source or the old record, exact physical payload
readback of a 20 MiB Blob after a full process restart (not just Blob metadata
sizes or a same-process Blob URL), one oversized
transaction, two simultaneous same-origin database transactions, committed
32 MiB and shared 128 MiB limits, origin isolation, deletion/reuse, and rebuilding
the quota baseline after restart. The large restart Blob uses the second
origin after the shared-port test, leaving the first origin's original 24 MiB
array baseline and quota thresholds unchanged. Deliberately filling the drive to the 16 MiB
reserve is **not** part of the guest suite; injected ledger boundaries and the
separate real native-volume probe cover those layers.

An interrupted run can leave only that token's test databases. Clean them with a
fresh report directory and the token printed in `result.json`:

```sh
python3 Tools/Symbian/tests/idb-quota/run.py "$BROWSER_EXE" \
  --phase cleanup --run TOKEN --origin http://localhost:8790 \
  --reports "$FRESH_REPORTS" --output "$CLEANUP_RESULTS"
```

Serve that fresh report directory too. Cleanup requires another independent
browser launch; it is not silently performed after a failed evidence run.
