# SameSite: real cross-site and restart acceptance

The final candidate passes **23 initial cases and 5 additional restart cases**
in two separate emulator processes. Both close normally with host exit 0 and no
panic. The browser SHA-256 is `1767da19ec00d9c823c60ff13d4f1c0a828d28cbd2abe97691f78c95ddeaca59`;
the emulator is `5f8704eb1c2461ceeb160eabd927a4f828765cad353e9107ae43f463127e292a`.
This run predates emulator TLB patch 0019.

The sites are HTTPS `localhost` and `127.0.0.1`, verified using a private CA and
a certificate with both DNS and binary IP SANs. A second localhost port is the
same-site control; HTTP localhost supplies the scheme-change cases. Production
CA bytes were restored afterwards. All fixture hashes remained unchanged for
the final two-process run, and the database was neither replaced nor deleted
between phases. No database snapshots or existing cookie values are archived.

The strict validator checks the exact case names and observed wire cookies:
Strict/Lax/default/None policy, DOM reads and writes, rejected cookie storage,
cross-site GET/POST, cross-site response cookies, WSS, A–B–A ancestor chains,
redirect taint, same-host scheme changes, and policy/session behavior after
process restart. Default cookies use Lax without the optional two-minute unsafe
POST compatibility window. Existing WebSocket response handling still cannot
store HttpOnly cookies; this suite does not claim that separate feature.

`result.json` contains process identities, timestamps, URLs, validation summaries
and CA/fixture hashes. The two phase directories retain reports, console logs
and screenshots. `final-raw/` and each historical run's `raw/` retain lossless
gzip copies of complete guest emulator/console/launcher logs, server logs and
wire reports, including the failures. `raw-index.json` records original lengths
and SHA-256 hashes plus compressed hashes. `independent-validation.json` reruns the real report validator
on the archived reports. `migration-preservation.json` separately confirms all
24 pre-existing rows retained their non-policy fields when moving schema 1→2;
that evidence came from candidate 7, whose policy suite still failed 5 cases.
Legacy, previously unenforced policy values are migrated to the defined Lax
default. Only hashed row identities, counts and booleans are retained.

Historical failures remain in `historical/`: numeric-IP family truncation
(candidate 2), missing SDK IP-SAN validation (5), duplicate migration-column
handling (6), inactive `PLATFORM(SYMBIAN)` guards (7), and two harness problems
(8 and 8-retry). The first candidate-8 run achieved 23/23 but the harness expected
a document URL where WebKit reports the external script URL. The retry achieved
23+5 but correctly failed its whole-directory hash check after an unrelated host
test changed. Neither historical failure is relabeled PASS. The final run uses
an explicit `COOKIE_FINAL_URL` record plus the exact script URL, fresh reports,
unique markers and unchanged hashes.

See [the reproducible fixture](../../../tests/cookie-samesite/README.md) and
[SDK TLS evidence](../tls-ip-san/README.md). These are emulator results, not a
claim of physical-phone validation or a complete browser cookie conformance
suite.
