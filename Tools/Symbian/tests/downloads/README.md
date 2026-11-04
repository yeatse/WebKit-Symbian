# Downloads

Checks suggested filenames, file writing, buffering and download-manager lifecycle.
Requires a host C++ compiler and Python 3.

```sh
export DOWNLOAD_TEST_BUILD=/tmp/download-tests
bash Tools/Symbian/tests/downloads/run-filename.sh
bash Tools/Symbian/tests/downloads/run-native-file-host.sh
bash Tools/Symbian/tests/downloads/run-buffer.sh
python3 Tools/Symbian/tests/downloads/run-deferred-queue.py \
  --build "$DOWNLOAD_TEST_BUILD/queue"
python3 Tools/Symbian/tests/downloads/run-manager.py \
  --build "$DOWNLOAD_TEST_BUILD/manager"
python3 Tools/Symbian/tests/downloads/test-server.py
```

Successful runs preserve payload data, complete queued transfers and release
resources through completion and cancellation.

For the browser save flow, run `server.py --port 8800 --log /tmp/downloads.jsonl`,
open `http://localhost:8800/`, choose a download and verify the saved payload.
