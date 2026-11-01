# Download component checks

These checks do not launch EKA or build the browser. Keep artifacts outside
the checkout:

```sh
export DOWNLOAD_TEST_BUILD=/path/to/download-tests
bash Tools/Symbian/tests/downloads/run-filename.sh
bash Tools/Symbian/tests/downloads/run-native-file-host.sh
bash Tools/Symbian/tests/downloads/run-buffer.sh
python3 Tools/Symbian/tests/downloads/run-deferred-queue.py \
    --build "$DOWNLOAD_TEST_BUILD/queue"
python3 Tools/Symbian/tests/downloads/run-manager.py \
    --build "$DOWNLOAD_TEST_BUILD/manager"
python3 Tools/Symbian/tests/downloads/test-server.py
```

- `filename.cpp`: 39 checks of the production UTF-16 suggestion/path helpers,
  including traversal, malformed surrogates, bounded writes and native length.
- `native-file.cpp`: 32 checks compile the actual `DownloadFileNative.cpp` with
  an injectable RFs/RFile substitute. Tests preserve existing/racing targets,
  propagate write/flush/rename failures, and verify cleanup ownership even when
  failed temporary creation returns an unrelated filename. They do not prove
  actual native file-server atomicity or disk-full behavior.
- `buffer.cpp`: 27 checks exercise the actual `CurlDownloadBuffer`, including a
  real producer/consumer pair transferring 50,000 chunks. This alone cannot
  prove that an engine callback releases its budget at the correct time.
- `run-deferred-queue.py`: extracts five current ResourceHandle method bodies
  and the actual `CompletionHandlerCallingScope` definition, records their
  hashes, and executes them with host type substitutes. Its 90 checks retain
  accounting through the second deferred queue and nested dispatch; exercise
  cancel while queued, cancel inside the sink, stale generation and detached
  client; and require buffer destruction before acknowledgement. This is not
  the complete engine, scheduler, refcounting implementation or native UI.
- `run-manager.py`: compiles the actual manager class and all its method bodies
  with host intrusive-reference, text, controller and UI substitutes. Its 26
  checks cover closing the view inside a save/progress callback, cancellation
  before starting the network request, stale cancellation IDs, rejection of a
  second transfer without disturbing the first, and native destination errors.
  It does not establish the actual firmware dialog or engine runtime behavior.

While the existing core files are held frozen for another build, the last
script accepts `--core-source-root /path/to/core-overlay`. The generated
`queue-source-identities.json` distinguishes that exact source from the active
checkout. Once the patch is applied, run the default command against the live
source. None of these counts is a guest browser-download result.

`server.py --port 8800 --peer-port 8801 --log /external/requests.jsonl`
serves `http://localhost:8800/?run=UNIQUE_TOKEN`. A second instance on 8801
tests cross-port redirects. The same handler can serve a TLS listener using
`--certificate /external/cert.pem --key /external/key.pem`; keep test CA keys
outside the checkout and use the existing strict temporary-CA restoration
procedure for guest tests. Do not disable verification for wrong-host or
untrusted-certificate cases. The host server test checks six fixture contracts,
including actual chunked/gzip bytes, truncation, redirects and POST logging.

Every normal payload is 524,288 bytes. The server prints its SHA-256 on startup
and records request method/body hash and whether credential headers arrived.
It distinguishes attachment-navigation links from `download=` links and a POST
form, so a response conversion that silently repeats POST cannot pass the
request-count check. The page deliberately emits no automatic PASS based just
on clicking a link: inspect the actual destination and request/cleanup evidence.

The native adapter and UI bridge also passed syntax checks using the browser's
actual SDK native-leave/Thumb/C++23 compile flags. The engine controller and core
path passed the current WebCore/Legacy ARM syntax commands. Full build,
native UI and HTTP(S) end-to-end acceptance remain pending; see
[DOWNLOADS.md](../../docs/DOWNLOADS.md).
