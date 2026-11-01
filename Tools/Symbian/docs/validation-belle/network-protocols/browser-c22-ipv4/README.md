# C22 IPv4 TLS h2 / H1 — actual browser PASS

Root executed fresh `net-101e726ceadc4edc9f1201e99e9da88c` on host0030 with
C22 browser d490e376f5008322de5f6a413811673c85f13f70eb98896283463820e869c2d9.
Unlike older host/OpenSSL tests, this uses the actual guest SDK mbedTLS/curl.

The server proves one TLS h2 connection for the page and twelve unique item
streams. Each body was checked byte-for-byte by the browser, and server SHA256
identities independently match the fixed data: 524288 + 11*16384 = 704512 bytes.
The actual active stream peak was **4**; the negotiated client limit was **8**,
with 262144-byte stream windows, push disabled and eight WINDOW_UPDATE events.
This establishes real multiplexing and replenished flow control, not a measured
peak of eight. The independent TLS H1 endpoint negotiated http/1.1 and returned
the exact fixed 23-byte body; all server errors remain empty.

The page/server paired validator, whole-log fatal scan, unforced host0, ordinary
own-PID Qt close after functional completion and 2s quiet, unchanged pins, and
restoration of whole WebKit profile/resources/CA/config/bindings/Qt all passed.
No panic is exempted. Raw host log stays external with its identity above; only
this fresh public page/fixture evidence and source harness are archived. No key,
original profile/configuration contents, credentials or binary is archived.

This proves controlled-loopback IPv4 h2/H1 normal behavior. It does not establish
AAAA/IPv6/WSS, cancellation, allocation exhaustion, official app EndTask or any
real-device IAP/TLS/performance claim. The separate known C21 CookieStore
EndTask failure remains FAIL. Original raw run and frozen harness are unchanged.
