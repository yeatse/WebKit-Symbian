# IndexedDB quota and Blob storage

Checks quota reservations, transaction accounting and committed Blob persistence.
Requires a host C++ compiler and the SQLite amalgamation used by the port.

```sh
Tools/Symbian/tests/idb-quota/run-ledger.sh
python3 Tools/Symbian/tests/idb-quota/run-producer.py --work /tmp/idb-producer
SQLITE_SOURCE=/path/to/sqlite-amalgamation-3350500 \
  Tools/Symbian/tests/idb-quota/run-blob.sh
```

Passing results reserve and release transaction space consistently, commit Blob
sidecars and preserve their contents when the database is reopened.
The browser database flow is described in [IndexedDB and Web Locks](../belle-backends/README.md).
