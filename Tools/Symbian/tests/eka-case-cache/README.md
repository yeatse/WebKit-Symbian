# Case-insensitive file lookup

Checks that newly created files are visible through EKA2L1's case-insensitive
filesystem lookup. Requires an EKA2L1 source tree and configured build directory.

```sh
python3 Tools/Symbian/tests/eka-case-cache/run.py \
  --eka-source /path/to/EKA2L1 --eka-build /path/to/EKA2L1/build \
  --output /tmp/case-cache-tests
```

The runner builds the filesystem fixture and records its results. Successful
lookups find files created after an earlier directory lookup.
