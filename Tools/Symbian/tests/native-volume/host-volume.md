# Host-backed volume accounting

Checks EKA2L1's filesystem volume accounting against host-backed files. Requires
an EKA2L1 checkout and a host C++ compiler.

```sh
python3 Tools/Symbian/tests/native-volume/run-host-volume.py \
  --eka-root /path/to/EKA2L1 --output /tmp/host-volume-tests
```

The runner creates temporary files and checks the reported capacity and free
space. Passing results track the bytes allocated and released by the fixture.
