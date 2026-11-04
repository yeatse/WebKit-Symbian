# Memory pressure

Checks the production memory-pressure policy and native observer lifecycle.
Requires a host C++ compiler.

```sh
bash Tools/Symbian/tests/memory-pressure/run-host.sh /tmp/memory-pressure
```

Passing results deliver pressure transitions, rearm notifications and release
native handles when the observer stops.

Build the native allocation and accounting probe with the Symbian SDK:

```sh
python3 Tools/Symbian/tests/memory-pressure/build-native.py \
  --sdk /path/to/sdk --output /tmp/memory-pressure-native --mode baseline
```

The probe checks committed page patterns and HAL accounting and writes
`C:/Data/memory-pressure-probe.log`. Native execution uses the setup in
[Testing](../../docs/TESTING.md).
