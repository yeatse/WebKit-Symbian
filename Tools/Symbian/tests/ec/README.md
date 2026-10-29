# EC WebCrypto regression

This bounded suite exercises the actual mbedTLS EC key, DER and operation helpers
for P-256, P-384 and P-521. The fixed WebKit base `4fb33923` supplies 12 independent
WPT ECDSA raw `r || s` signatures (three curves and SHA-1/256/384/512), plus three
ECDH key pairs and known shared secrets. `prepare-vectors.py` records the original
Git blobs and SHA256 values without changing the sparse checkout.

The helper suite performs 244 checks: known answers, signatures and exact field
widths, raw/JWK components, exact standard SPKI and PKCS8 output, optional SEC1
curve/public fields, optional PKCS8 attributes, ordinary malformed formats and
lengths, invalid curve points and scalars, entropy failure and four concurrent
128 KiB threads operating on the same key. P-521 signatures are 132 bytes and
its known ECDH result includes the leading zero in the full 66-byte field width.
All entropy-failure callbacks are confined to explicit negative cases.

```sh
python3 Tools/Symbian/tests/ec/run-host.py --work "$EC_WORK/host" \
  --mbedtls-source /path/to/sdk-mbedtls-source \
  --mbedcrypto /path/to/native-host/libmbedcrypto.a \
  --abseil-include /path/to/abseil/include
python3 Tools/Symbian/tests/ec/test-verify.py
```

For an overlay outside the checkout, pass `--checkout /path/to/WebKit` to
`run-host.py` and `prepare-vectors.py`. `run-host.py` also accepts `--cxx`, repeatable
`--cxx-flag`, and a `--runner` command prefix for an already available ARM Linux
cross compiler and QEMU. Commands use no shell evaluation. Its 120-second runtime
deadline and source/archive hashes are recorded in `result.json`.

```sh
python3 Tools/Symbian/tests/ec/run-host.py --work "$EC_WORK/arm1136" \
  --mbedtls-source /path/to/sdk-mbedtls-source \
  --mbedcrypto /path/to/arm-linux/libmbedcrypto.a \
  --abseil-include /path/to/abseil/include --cxx arm-linux-gnueabi-g++ \
  --cxx-flag=--sysroot=/path/to/sysroot --cxx-flag=-march=armv6 \
  --cxx-flag=-marm --cxx-flag=-mfpu=vfp --cxx-flag=-mfloat-abi=softfp \
  --runner 'qemu-arm -L /path/to/sysroot/usr/arm-linux-gnueabi -cpu arm1136'
```

Host and ARM Linux/QEMU results prove helper behavior, not Symbian E32 or browser
integration. No EKA runtime or full-engine build is performed by these scripts.

The browser fixture contains 30 named cases, including all 15 known answers,
key formats, JWK/usage/extractability/curve/hash rejection, key generation,
same-key concurrency and actual DedicatedWorker signing and derivation. Worker
keys are independently imported and no CryptoKey is transferred. CryptoKey
structured cloning and IndexedDB persistence remain unsupported.

ECDH length tests preserve the existing shared WebKit implementation: `null`
returns the complete secret, `0` returns empty, and positive lengths use
`ceil(bits / 8)` bytes without masking the final byte (WebCrypto issue 369).
Thus P-521 supports its full 528-bit byte representation and rejects 529 bits;
P-256 rejects 257 and P-384 rejects 385. This is inherited frontend behavior,
not an EC backend change.

```sh
python3 Tools/Symbian/tests/ec/prepare-vectors.py --work "$EC_WORK/page"
cp Tools/Symbian/tests/ec/{index.html,ec-tests.js,page.js,worker.js} "$EC_WORK/page/"
# Serve the page on a disposable localhost origin through the existing owner.
# Open /?run=UNIQUE_TOKEN using the exclusive browser/emulator runner.
python3 Tools/Symbian/tests/ec/verify.py "$FRESH_GUEST_LOG" \
  --run "$RUN_TOKEN" --vectors "$EC_WORK/page/vectors.json"
```

The page has a 180-second deadline and the worker has a 30-second deadline.
Runtime acceptance requires the strict launcher to exit normally, a fresh log,
30 ordered PASS case markers, exactly one matching `EC_RESULT` report and
`EC_DONE run=TOKEN passed=30 total=30`. Preserve executable/library hashes and
guest/host logs with that report. Feature detection alone is insufficient.
