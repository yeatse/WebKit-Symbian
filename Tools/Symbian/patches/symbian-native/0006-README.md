# ECom plugin identities

Patch `0006-Support-explicit-ECom-plugin-identities.patch` adds ECom DLL
conversion to `symbian-platform` v0.3.0.

The converter accepts `--uid2 0x10009d8d` with the plugin's assigned UID3.
The C++ interface exposes the same operation as `ConvertDllWithUid2`.
Ordinary DLLs use UID2 `0x1000008d`.

Apply the [native SDK patch stack](README.md) through
`Tools/Symbian/scripts/build-symbian-native.sh`. Its E32 test target includes
plugin identity coverage.

The patch and its tests use the SDK's Apache-2.0 license.
