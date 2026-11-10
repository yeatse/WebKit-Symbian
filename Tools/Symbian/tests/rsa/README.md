# RSA WebCrypto

Exercises RSA-OAEP encryption, RSA-PSS and PKCS#1 signatures, and key encodings.
Requires patched mbedTLS sources, a host `libmbedcrypto.a` and Abseil headers.

```sh
python3 Tools/Symbian/tests/rsa/run-host.py --work /tmp/rsa-tests \
  --mbedtls-source /path/to/mbedtls \
  --mbedcrypto /path/to/libmbedcrypto.a \
  --abseil-include /path/to/abseil/include
```

The runner compiles the production crypto helpers and checks encryption,
signatures and imported/exported keys. It exits successfully when all checks pass.
