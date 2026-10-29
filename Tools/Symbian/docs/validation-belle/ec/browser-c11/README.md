# EC in the actual Belle browser

**30/30 passed** on [candidate 11](../../candidate11/README.md). This includes
P-256/P-384/P-521 ECDSA/ECDH known answers, exact field widths, key formats,
generation, concurrent operations and an actual DedicatedWorker. The complete
report validator and normal browser observation runner both exited zero.

The fresh guest log, final screenshot, unmodified compressed host/driver logs,
full result and [file identities](manifest.json) are retained. All recorded
input identities remained unchanged. This is an emulator browser result,
not phone acceptance. The inherited ECDH length behavior and the lack of
CryptoKey structured cloning/persistent wrapping remain as documented in
the [EC suite](../../../../tests/ec/README.md).
