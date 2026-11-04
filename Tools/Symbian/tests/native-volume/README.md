# Native disk-volume accounting

Builds the production filesystem bridge and checks `RFs::Volume` capacity and
free-space reporting. The fixture uses C, E and read-only Z drives, plus an
unmounted drive for its error check.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native
export PROBE_BUILD=/tmp/native-volume
Tools/Symbian/tests/native-volume/build.sh
```

Run `e32/native_volume.exe` from the build directory using the native probe setup
in [Testing](../../docs/TESTING.md). The probe checks 64-bit volume values and the
free-space change while writing and deleting a 2 MiB temporary file.

A successful run reports `NATIVE_VOLUME_RESULT 10/10 PASS`. The
[host-backed volume check](host-volume.md) exercises EKA2L1's capacity accounting.
