# RSA WebCrypto regression

This suite tests the actual mbedTLS RSA key/DER/operation helpers, then the
browser's complete RSA-OAEP, RSA-PSS and RSASSA-PKCS1-v1_5 paths. EC algorithms
have a separate suite in `../ec`. CryptoKey structured cloning, worker key transfer and
IndexedDB key persistence remain a separate CryptoClient/platform key-wrapping
task; the worker test imports its own key. No raw serialized private-key storage
has been enabled by this change.

The fixed WebKit base `4fb33923` contains 24 independent WPT vectors covering
SHA-1/256/384/512, OAEP labels, RSASSA exact signatures and PSS signed data.
`prepare-vectors.py` records each original Git blob and SHA256, and creates the
native header plus browser JSON. It does not alter the sparse checkout.

The host test compiles the production helpers. It includes known-answer
comparisons, true PKCS8 wrapping, minimal private keys, corrupted CRT fields,
DER length/trailing/type checks, optional PKCS8 attributes with bounded nesting, exact PSS salt bounds (including digest+1 and
UINT32_MAX), OAEP maximum plaintext and ciphertext lengths, injected entropy
failure and concurrent operations using four 128 KiB pthreads. Native host or
QEMU results alone do not establish Symbian runtime support.

```sh
python3 Tools/Symbian/tests/rsa/run-host.py --work "$RSA_WORK/host" \
  --mbedtls-source /path/to/sdk-mbedtls-source \
  --mbedcrypto /path/to/native-host/libmbedcrypto.a \
  --abseil-include /path/to/abseil/include
python3 Tools/Symbian/tests/rsa/test-verify.py
```

The same helper test is built as an E32 executable against the installed ARMv6
`libmbedcrypto.a` and the production WTF entropy bridge. It uses actual SDK
hardware entropy, including for every private operation's blinding. The
injected failure callback is used only in explicit negative tests.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/symbian-sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native
export WEBKIT_BUILD=/path/to/existing/webcore-build
PROBE_BUILD="$RSA_WORK/native" bash Tools/Symbian/tests/rsa/build-native.sh
```

Run `e32/rsa_probe.exe` without arguments through the sole EKA owner's strict
native runner. The fresh guest log is `C:/Data/WebKit/rsa-probe.log`; success
requires all 171 checks, `PASS: rsa-probe 171/171`, `rsa-probe exiting 0`, normal
guest/host exit and no panic. Use a bounded 120-second watchdog. The executable
uses UID `0xe0000902`; it does not replace any browser or native DLL.

For the browser, stage the generated vectors beside the page and serve them on
a disposable localhost origin. Generation is included in the 180-second page
deadline. Use a new token and fresh logs for every launch:

```sh
python3 Tools/Symbian/tests/rsa/prepare-vectors.py --work "$RSA_WORK/page"
cp Tools/Symbian/tests/rsa/{index.html,page.js,worker.js} "$RSA_WORK/page/"
python3 -m http.server 8805 --bind 0.0.0.0 --directory "$RSA_WORK/page"
# In the exclusive emulator environment, with RUN_TOKEN uniquely generated:
WAIT=190 EKA2L1_EXPECT_PASS="RSA_DONE run=$RUN_TOKEN passed=33 total=33" \
  Tools/Symbian/eka2l1/run-browser.sh "$BROWSER_EXE" \
  "http://localhost:8805/?run=$RUN_TOKEN"
python3 Tools/Symbian/tests/rsa/verify.py "$FRESH_GUEST_LOG" \
  --run "$RUN_TOKEN" --vectors "$RSA_WORK/page/vectors.json"
```

The launcher must finish normally before accepting its report. Its exit status,
fresh guest/host logs, executable/library hashes and `verify.py` output belong
in the runtime evidence. The 33 browser cases cover the 24 fixed vectors plus
generation, all key formats, malformed JWK/DER, exact salt limits, OAEP wrapping,
usage/extractability errors, same-key concurrency and actual DedicatedWorker
operations. Feature detection and a matching RSA algorithm name are insufficient.
