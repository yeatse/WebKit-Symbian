# Aligned allocation under pressure

Builds the production aligned allocator and memory bridge into a native probe.
The probe retains 80 MiB of aligned blocks, checks their contents and exercises
reuse and concurrent allocation.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native
export PROBE_BUILD=/tmp/aligned-pressure
Tools/Symbian/tests/aligned-pressure/build.sh
```

Run `e32/aligned_pressure.exe` from the build directory with the native probe
runner described in [Testing](../../docs/TESTING.md). The probe takes no arguments
and writes `C:/Data/WebKit/aligned-pressure.log`. A passing run retains 83,886,080
bytes with intact page patterns, reuses released blocks and exits successfully.
