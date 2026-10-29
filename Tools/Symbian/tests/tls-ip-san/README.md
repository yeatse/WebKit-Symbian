# IP certificate identities and SNI

These tests use the actual SDK mbedTLS source built as host libraries. They do
not substitute the system's newer mbedTLS, replace verification callbacks,
clear verification flags, or make network requests.

```sh
./make-certificates.py /tmp/private-ip-san-certificates
./build-host.sh /path/to/mbedtls-source /path/to/host/tls/library \
    /path/to/sdk/include/abseil /tmp/ip-san-probes
./run.py --probe /tmp/ip-san-probes/probe \
    --fixtures /tmp/private-ip-san-certificates --output /tmp/ip-san-result
./run-parser.py --probe /tmp/ip-san-probes/parser --output /tmp/ip-parser-result
```

The certificate generator requires Python `cryptography`. It creates private
CA-signed DER/PEM fixtures, never exports a private key, refuses to overwrite a
directory, and records hashes. Positive certificates expire after two days;
generate fresh fixtures when rerunning. Use the identical fixtures for old/new
library comparisons.

`probe.c` parses and verifies the signed certificates through
`mbedtls_x509_crt_verify` with the original expected hostname. The 25 cases cover
exact IPv4/IPv6 binary SANs, equivalent IPv6 spellings, IPv4-mapped IPv6,
different IP addresses, numeric DNS/CN impersonation (including a DNS match
alongside the wrong IP SAN), ordinary DNS/wildcards, wrong CA, expired/future
certificates and combined name/chain/time errors. Every negative case requires
the exact expected flags, and every positive case requires none.

Ten further cases call the actual `mbedtls_ssl_set_hostname` and handshake
code, capturing complete ClientHello records through an in-memory BIO. The
runner parses their extension lengths and unique extension types: DNS SNI must
remain, while IPv4/IPv6 SNI must be absent, for both TLS 1.2 and TLS 1.3. The
hostname remains set for subsequent certificate verification.

The separate parser test includes the actual SDK internal helper, comparing
its output against host `inet_pton`. Named edge cases, every zero-compression
position/length, dotted IPv4 tails and deterministic mutations cover the
portable parser's boundaries. In particular, seven groups followed by `::`
and six groups followed by an IPv4 tail are valid; eight groups followed by an
extra colon are not. Destination guard bytes detect out-of-range writes.

Recorded stock SDK results are **14/35**, confirming both rejected valid IP
SANs and wrongly accepted numeric DNS/CN identities. The repaired source is
**35/35**, with **2,069 parser cases and zero differences**. These host results
do not establish guest TLS transport, proxy behavior, all certificate formats,
or complete TLS conformance. The browser must separately pass strict IP-SAN
HTTPS, its existing TLS/WSS rejection tests, and the real cross-site cookie
suite. IPv6 identity tests do not enable curl's currently disabled IPv6
transport.
