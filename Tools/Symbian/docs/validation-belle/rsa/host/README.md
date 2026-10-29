# RSA helper verification

The production RSA import, DER and operation helpers pass **153 checks on the
native host and 153 on QEMU ARM1136 with 32-bit pointers**. These include 24
fixed upstream known-answer vectors, malformed CRT/DER rejection, entropy
failure, exact salt/length boundaries and four concurrent 128 KiB pthreads.
The seven report-contract checks reject incomplete or stale browser evidence.

`vector-sources.json` identifies the original files at fixed base `4fb33923`.
`result.json` records production helper, library, log and E32 probe hashes. The
QEMU library is rebuilt from the SDK's mbedTLS source for ARM Linux; it is not
the Symbian E32 library. The separate [`native` evidence](../native/README.md)
records the real SDK probe's completed emulator run.

The two WebCore translation units pass the actual Symbian compiler with a
temporary key-container header overlay and PSS macro enabled. Integration into
the existing key header, registry and build remains pending the active quota
candidate's freeze boundary. **No browser or E32 runtime RSA support is claimed
by this host evidence directory.**

Reproduction, native probe instructions and the 33-case browser fixture are in
[`tests/rsa/README.md`](../../../../tests/rsa/README.md). CryptoKey cloning and
persistence remain a separate platform-client/key-wrapping task.
