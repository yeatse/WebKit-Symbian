# Elliptic-curve WebCrypto

Exercises P-256, P-384 and P-521 ECDSA/ECDH operations and key import/export.
Requires the patched mbedTLS sources, a host `libmbedcrypto.a` and Abseil headers.

```sh
python3 Tools/Symbian/tests/ec/run-host.py --work /tmp/ec-tests \
  --mbedtls-source /path/to/mbedtls \
  --mbedcrypto /path/to/libmbedcrypto.a \
  --abseil-include /path/to/abseil/include
```

The runner compiles the production crypto code and checks signatures, shared
secrets and key encodings. It exits successfully when all checks pass.
