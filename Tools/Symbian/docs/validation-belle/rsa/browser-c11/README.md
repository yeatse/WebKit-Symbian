# RSA in the actual Belle browser

**33/33 passed** on [candidate 11](../../candidate11/README.md), including the
24 fixed upstream vectors, key generation and formats, OAEP/PSS/RSASSA,
parallel operations and an actual DedicatedWorker. The page completed in
32,999 ms. The runner then observed five more seconds and closed normally;
its exit status and the independent complete-report validator were both zero.

The fresh guest log, final screenshot, unmodified compressed host/driver logs,
full result and [file identities](manifest.json) are retained. The image,
resources, ICU data, mbedcrypto archive and fixtures remained unchanged during
the run. This is an emulator browser result, not phone acceptance.
CryptoKey structured cloning and persistent key wrapping remain unsupported.
