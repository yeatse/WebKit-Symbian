# Native disk-volume accounting

This probe compiles the actual `FileSystemSymbianNative.cpp` bridge and calls
`RFs::Volume` on Symbian. It uses no synthetic capacity in the native code.
Build with SDK 0.3.0 and the converter already used for the browser:

```sh
SYMBIAN_SDK_PREFIX=/path/to/sdk \
SYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native \
PROBE_BUILD=/path/to/native-volume-build ./build.sh
```

The runtime owner launches `e32/native_volume.exe` (UID `0xe00008f7`) under the
normal exclusive emulator locks, captures fresh output and requires
`NATIVE_VOLUME_RESULT 10/10 PASS`, normal process/host exit and no panic. The
configured validation device must expose C, E and read-only Z; one unmounted
drive is needed for the error control. The probe tests native 64-bit capacity,
read-only free space, session-default drive selection, invalid/unmounted failures with unchanged output values,
and the available-space decrease after flushing a unique 2 MiB temporary file
on C. It deletes only that uniquely created file. Record the host mapping and
its actual filesystem capacity alongside the guest values.

Run the write/decrease check without concurrent host builds, downloads or other
guest writers: emulator drives share their underlying host volume. The check
must fail against EKA's former constant-1-GiB implementation; a successful
`RFs::Volume` call by itself is insufficient. No guest result is asserted until
the fixed host and this exact image have run together.

`../native-filesystem/run.sh` separately tests the real WTF path adapter on the
host with only the native volume call injected. Those tests verify relative
paths use Open C's current drive, while absolute C/E paths select their own
drive and 64-bit byte counts remain intact. They do not substitute for this
native probe or for actual low-disk IndexedDB behavior.
