# EKA2L1 descriptor file I/O

Exercises descriptor lengths and file IPC with a 34 MiB data round trip. Requires
an EKA2L1 checkout and a C++ compiler supporting AddressSanitizer and UBSan.

```sh
python3 Tools/Symbian/tests/eka-file-io/run-host.py \
  --eka /path/to/EKA2L1 --work /tmp/eka-file-io --sanitize
```

The runner compiles the descriptor and file IPC implementations with a temporary
filesystem fixture. Passing results preserve descriptor lengths and every byte of
the written data, with correct read lengths and native error returns.
