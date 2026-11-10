# Network state notifications

Checks connection-monitor subscriptions and online/offline state propagation.
Requires a host C++ compiler.

```sh
python3 Tools/Symbian/tests/network-state/run-host.py \
  --output /tmp/network-state-tests
```

The runner builds the production observer fixture. Passing results notify
listeners of state transitions and release subscriptions when observers close.
