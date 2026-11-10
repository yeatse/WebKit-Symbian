# IP certificate verification

The SDK uses Mbed TLS 3.4.1 with IP address support for certificate identities.
IPv4 and IPv6 reference addresses match binary `iPAddress` Subject Alternative
Names. DNS hostnames use DNS certificate matching and SNI. Certificate-chain
and hostname verification are enabled.

From the checkout root:

```sh
python3 Tools/Symbian/scripts/rebuild-sdk-mbedtls-ip.py \
  --sdk "$SYMBIAN_SDK_PREFIX" --install
```

The helper uses `SYMBIAN_WEBKIT_WORK/sdk-mbedtls-ip`; `--work-dir` selects a
custom directory. It builds the ARMv6 certificate and TLS client objects,
runs certificate and ClientHello fixtures, and installs `libmbedx509.a` and
`libmbedtls.a`.

Requirements are Python 3.12 with `cryptography`, CMake 3.28+, Ninja, `patch`,
host C/C++ compilers and binutils. Input versions are recorded in
`manifest.json`.

The address parser is adapted from
[Mbed TLS 3.6.7](https://github.com/Mbed-TLS/mbedtls/blob/068ff080b369adfac81509f9b57b2afabaf82dc5/library/x509_crt.c).
Its original copyright and license notice are retained. See the
[test guide](../../../docs/TESTING.md) for the TLS workflow.
