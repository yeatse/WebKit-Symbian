# SameSite cookie regression

This suite uses **localhost and 127.0.0.1 as different sites**, backed by the
same local server. A second localhost port is the same-site/cross-origin
control. No external service or DNS override is needed. The HTTPS certificate
contains both a DNS localhost SAN and an IP 127.0.0.1 SAN; using a DNS-only
certificate for the second site would test TLS failure instead of cookie policy.

The port inherits an upstream curl limitation: the original parser discarded
SameSite, the v1 database did not store it, and header selection discarded
SameSiteInfo. This is not a regression introduced by the Symbian migration.

The repaired policy is default Lax, Strict only on same-site requests, Lax also
on safe top-level navigations, and None only with Secure. **There is no two-minute
unsafe-POST compatibility exception.** A cookie with an absent or unrecognized
attribute uses Lax, including existing v1 rows whose original attribute cannot
be recovered. Schema v2 adds the policy in a transaction without deleting those
rows. DOM access does not receive the safe-navigation exception. A WebSocket is
a subresource; its cookie URL uses HTTP(S), not WS(S). Redirect chains retain
cross-site classification after crossing a site, including an eventual return.

Run the host checks without an emulator:

```sh
python3 Tools/Symbian/tests/cookie-samesite/test-host.py --cxx c++ -v
node --check Tools/Symbian/tests/cookie-samesite/case.js
```

The C++ check compiles the **production** CookieSameSite.h decision functions
with only Cookie/SameSiteInfo data-declaration shims. SQLite tests execute the
production schema, ALTER and INSERT SQL, including rollback and reopen. Fixture
tests preserve observed headers and FAIL reports. Validator tests reject missing,
duplicated, stale and failed outcomes, and wrong raw cookies despite a claimed
PASS. These checks do **not** run
WebCore's String parser, SQLite C++ wrapper, request context creation or guest
storage; browser validation below is still required.

Prepare independent local fixtures (paths are examples):

```sh
bash Tools/Symbian/tests/cookie-samesite/make-certificates.sh /tmp/samesite-tls
python3 Tools/Symbian/tests/cookie-samesite/server.py \
  --tls /tmp/samesite-tls --output /tmp/samesite-results
```

The server binds loopback only and uses HTTPS 8791/8792 plus HTTP 8790. Give the
runtime owner `ca.crt`; temporarily append only that test root to a backed-up
isolated guest CA bundle, restoring the exact bytes after the run. Do not modify
SDK/release trust, skip TLS verification, replace a running emulator, or install
test certificates on a user's device. Fresh certificate dates must include the
guest clock.

Launch `https://localhost:8791/start?run=UNIQUE_ID` in the actual browser. The page
automatically seeds both sites, executes fetch, iframe, DOM and WebSocket cases,
then performs **real top-level GET and form POST navigations**. It covers Strict,
Lax, default and None; fresh-default POST rejection; invalid None without Secure;
cross-site DOM/response-set restrictions; mixed-case, unrecognized and duplicate
attributes; 307 POST; A→B→A redirects and nested iframe ancestry; and HTTP→HTTPS
schemeful GET/POST. It ends at `/done` and logs `PASS: cookie-samesite N/N` only
if all **23 initial** outcomes passed exactly once. The JSON includes raw request
method, host, query, TLS flag, Origin and Cookie headers so an empty response or a network failure cannot be
mistaken for successful cookie filtering.

Then **close the browser process**, retain the same profile/database and server
reports, restart and load `https://localhost:8791/resume?run=UNIQUE_ID`. This checks
both sites' persisted policies, removal of session cookies, and continued
cross-site restrictions. Do not run `/start` again before `/resume`: reseeding
would hide a persistence failure. The initial and resumed outcomes append to
`UNIQUE_ID.json`; distinguish the two stages using the `phase` field. The resumed
report must contain all 23 initial plus exactly 5 resumed outcomes, **28 total**.

`validate.py` exports `validate_initial(report_or_path, token, not_before_ns=None)`
and `validate_resumed(...)`; each returns a summary or raises. It requires the
exact case names/phases, token and raw two-site/POST/WebSocket/scheme observations,
not just an aggregate PASS marker. A runtime runner must independently require
normal guest closure and actual process restart, and keep the launch timestamp
for freshness validation. The validator never starts a browser or alters trust.

```sh
python3 Tools/Symbian/tests/cookie-samesite/validate.py /tmp/samesite-results/UNIQUE_ID.json \
  --run UNIQUE_ID --phase resumed --not-before-ns INITIAL_HOST_START_NS
```

Keep separate old-engine and repaired-engine runs with fresh IDs and the same
fixtures. An old build's wrong-cookie outcomes are the negative control. Retain
browser executable hash, source commit, CA hash, raw logs, report JSON, and exact
database backup/restore steps. Existing network.html and TLS/WSS tests must also
pass; this suite does not replace those regressions or validate Cookie Store API.

Status: production policy/SQL and fixture/validator host checks pass; six changed
production translation units pass ARMv6 syntax checking. Guest acceptance is
pending. No guest PASS is implied by the presence of this directory.
