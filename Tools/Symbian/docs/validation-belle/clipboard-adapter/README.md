# System clipboard adapter

The actual `ClipboardNative.cpp` adapter passes **31/31 native checks** on the
emulator with patches through 0026. Both guest and host exit normally with code
zero; the original clipboard's absence is restored. See the
[fresh result](native-after-0026/result.json),
[guest checks](native-after-0026/clipboard-adapter-probe.log), and
[evidence identities](native-after-0026/manifest.json).

The probe image is unchanged from the [first failed run](native-before/README.md):
`662db66d1e3543af89fef1d73cbd8c804c77a82602b355e25cf28f9afc96aeac`.
The new emulator is
`0d2714494b322684872c0fc036c2f206b8514e54d021dc2ec1e3182af5855b98`.
The three previously failing read-only/exclusive-access checks now pass after
the file-service changes in patch 0025. This does not establish the cause of
the earlier host shutdown failure. That original result remains failed.

The host output is stored as gzip without changing its bytes. No original
clipboard data or guest database is included. An earlier invocation of the
new runner omitted the emulator environment variable and stopped at argument
validation before launching a guest; its preflight failure is retained separately.

The [24 host boundary checks](host/README.md) and these native checks cover
the adapter, Unicode/plain-text storage and error handling. Browser editing,
native menu/keyboard routing and cross-application interoperability still
require their separate acceptance runs. No DOM paste permission is granted.
