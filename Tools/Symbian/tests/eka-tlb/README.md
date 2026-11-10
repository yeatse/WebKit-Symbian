# ARM memory translation

Exercises EKA2L1's ARM and Thumb memory accesses through translated memory.
Requires the EKA2L1 source tree and its configured build directory.

```sh
python3 Tools/Symbian/tests/eka-tlb/build.py \
  --eka-source /path/to/EKA2L1 --eka-build /path/to/EKA2L1/build \
  --output /tmp/tlb-tests
```

The runner builds the memory-access fixtures and records their results in the
output directory. Passing cases read and write the expected mapped bytes.
