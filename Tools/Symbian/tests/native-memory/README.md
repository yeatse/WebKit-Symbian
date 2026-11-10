# Native memory allocation

Checks the production memory bridge and aligned allocator: reserve, commit,
decommit, reuse and release. Requires a host C++ compiler and Python 3.

```sh
bash Tools/Symbian/tests/native-memory/run.sh
python3 Tools/Symbian/tests/native-memory/run-aligned-pool.py
```

Passing results preserve live allocation patterns, zero recommitted pages and
release owned regions. The allocator test also checks `fastFree` routing for
libc, SDK-aligned and native GC allocations. The [aligned pressure probe](../aligned-pressure/README.md)
checks sustained native allocation with the Symbian SDK.
