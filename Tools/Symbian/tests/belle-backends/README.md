# IndexedDB and Web Locks

Runs the browser's persistent database and lock coordination flows across pages
and workers. Use the EKA2L1 setup described in [Testing](../../docs/TESTING.md).

Start the fixture server:

```sh
python3 Tools/Symbian/tests/belle-storage/server.py --suite backends \
  --reports /tmp/backend-reports
```

In another terminal, run the browser:

```sh
python3 Tools/Symbian/tests/belle-storage/run.py /path/to/browser.exe \
  --suite backends --reports /tmp/backend-reports --output /tmp/backend-results
```

The result directory contains the page reports and browser logs. Passing flows
commit and reopen database records and coordinate competing Web Locks.
