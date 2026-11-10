# ALF cancellation

Exercises ALF request cancellation, completion and session teardown using the
EKA2L1 server sources. Requires a host C++ compiler and an EKA2L1 checkout.

```sh
python3 Tools/Symbian/tests/alf-cancel/run-host.py \
  --eka-root /path/to/EKA2L1 --output /tmp/alf-cancel-tests
```

The runner builds the fixture and writes its result to the output directory.
A successful run completes all cancellation and lifecycle checks.
