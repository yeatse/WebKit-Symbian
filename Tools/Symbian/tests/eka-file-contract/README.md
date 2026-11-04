# EKA2L1 file sharing and attributes

Checks file sharing, read-only attributes and persistent filesystem metadata.
Requires an EKA2L1 source tree and configured build directory.

```sh
python3 Tools/Symbian/tests/eka-file-contract/run.py \
  --eka-source /path/to/EKA2L1 --eka-build /path/to/EKA2L1/build \
  --output /tmp/file-contract-tests
```

The runner writes the build and results to the output directory. Passing checks
confirm that file operations enforce sharing and preserve the expected metadata.
