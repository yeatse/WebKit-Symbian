# SDK IP-SAN and SNI regression

Using the exact SDK 3.4.1 source built for the host, the original libraries pass
14 of 35 strict identity/ClientHello checks; the repaired libraries pass all 35.
The original verifier rejects valid IP SANs and can accept a numeric identity
from a DNS SAN or CN. It also sends literal IP addresses in SNI. The repair
uses binary `iPAddress` SAN matching for numeric identities, preserves all
other verification flags and leaves DNS verification unchanged. SNI omits IP
literals while the expected hostname remains available for verification.

The evidence includes CA-signed public DER/PEM certificates (no private keys),
old and corrected per-case JSON, raw output, and actual TLS 1.2/1.3 ClientHello
bytes emitted by mbedTLS into an in-memory BIO. The test makes no network
requests and installs no verification callback. The manifest records the three
repaired SDK source hashes and every evidence file.

Independent parser review caught an inherited eight-group IPv6 trailing-colon
acceptance bug before installation. The final helper passes 2,069 named and
deterministic differential cases against host `inet_pton`; the full inputs and
result are in `parser/`. A larger scratch run also passed 24,015 cases after
the correction, but the smaller portable set is the maintained regression.

See [the reproducible tests](../../../tests/tls-ip-san/README.md) for build and
execution commands. Positive fixture certificates expire after two days;
regenerate them for later runs. Host validation does not establish guest TLS
transport or full TLS conformance. Corrected browser IP HTTPS, existing TLS/WSS
negatives and the two-process cross-site cookie fixture still require the
runtime owner's separate guest evidence.
