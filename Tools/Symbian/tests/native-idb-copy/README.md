# Native database file copying

Builds the production filesystem copy function with OpenC into a native probe.
The fixture copies binary payloads and checks the resulting files.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native
export PROBE_BUILD=/tmp/native-idb-copy
Tools/Symbian/tests/native-idb-copy/build.sh
```

Run `e32/idb_copy_probe.exe` from the build directory using the native probe
setup in [Testing](../../docs/TESTING.md). The probe writes
`C:/Data/WebKit/idb-copy-probe.log`. A passing run copies the exact payload,
cleans up its temporary files and finishes with `idb-copy-probe exiting 0`.
