# RSA optional attributes: Symbian runtime

The updated E32 probe passed **171/171** on the X7-00 firmware in EKA2L1.
Guest and host both exited 0 with no panic or access violation. The four
concurrent RSA threads each used 128 KiB, confirmed by the host excerpt.

`rsa-probe.log` and `result.json` are unchanged copies of the actual run.
`manifest.json` binds them to the executable, installed SDK mbedTLS archive,
production helper sources and emulator. The private runner change from the
earlier 153-check run is preserved as a patch; it only updates the expected
count. The shared runner was not changed for this test.

The original [153-check run](../native/README.md) remains intact. The separate
[host and ARM1136 checks](../host-attributes/README.md) describe the 18 added
PKCS8 attribute cases and retain the exact source/harness delta.

This validates actual SDK RSA operations and entropy in Symbian. It does not
yet establish that the browser exposes RSA through WebCrypto; that requires
the pending registry/key integration and the independent 33-case browser run.
