# Native large-file I/O

Builds a Symbian probe for OpenC reads/writes and file-server IPC. The fixture
uses a 37 MiB buffer and checks complete file contents after each transfer.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native
export PROBE_BUILD=/tmp/native-large-file
Tools/Symbian/tests/native-large-file/build.sh
```

Run `e32/large_file_probe.exe` from the build directory with the native probe
setup in [Testing](../../docs/TESTING.md), allowing 120 seconds. It writes
`C:/Data/WebKit/large-file-probe.log`. Passing results preserve every payload
byte and finish with `large-file-probe exiting 0`.
