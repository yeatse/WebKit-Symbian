# Browser storage and messaging

Checks localStorage and IndexedDB persistence, storage events and BroadcastChannel
messages between pages, frames and workers. The server provides two test origins.
Use the EKA2L1 setup in [Testing](../../docs/TESTING.md).

```sh
python3 Tools/Symbian/tests/belle-storage/server.py --reports /tmp/storage-reports
```

Run the browser in another terminal:

```sh
python3 Tools/Symbian/tests/belle-storage/run.py /path/to/browser.exe \
  --reports /tmp/storage-reports --output /tmp/storage-results
```

The runner collects page reports and logs. Passing results confirm data survives
reopening and messages reach the intended contexts.
