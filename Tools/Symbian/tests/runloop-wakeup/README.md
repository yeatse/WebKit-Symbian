# Run-loop wakeups

Checks cross-thread run-loop wakeups and callback scheduling. Requires Python 3
and a host C++ compiler.

```sh
python3 Tools/Symbian/tests/runloop-wakeup/run.py --output /tmp/runloop-wakeup
```

The runner builds the production wakeup code with its scheduler fixture. Passing
results dispatch registered work and cleanly cancel or release pending callbacks.
