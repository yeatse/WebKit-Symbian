# Browser network fixtures

Run `python3 Tools/Symbian/tests/serve-network-tests.py --reports /path/to/results` and load `http://localhost:8765/network.html?autorun=1` in the emulator browser. Two HTTP ports, 8765/8766, test origin boundaries. The emulator's socket bridge connects to host loopback. The server defaults to loopback and needs no external service.

The page logs 23 named outcomes and a `PASS: network 23/23` summary, and POSTs a JSON report to the server. Tests cover fetch, async/sync XHR, chunked/data responses, POST redirect rules, async/sync 401 refusal, cancellation, cookie paths/recalculation/deletion, cross-port authorization and cookies, WebSocket text/binary, HTTP-to-local/forbidden-port redirect rejection, and recovery after failure. A frozen synchronous XHR requires a host-side timeout; a JavaScript timeout cannot interrupt it. Require all outcomes from the current browser run and scan the emulator for guest panics/access violations. Server-side fixture smoke tests are not browser validation.

## TLS fixtures

```sh
Tools/Symbian/tests/make-test-certificates.sh /path/to/new-test-tls
python3 Tools/Symbian/tests/serve-network-tests.py --tls-dir /path/to/new-test-tls --reports /path/to/results
```

Certificates are freshly generated, valid for two days, and must overlap the guest's clock. The trusted endpoint is `https://localhost:8767`; port 8768 has the same test CA but a wrong hostname, and 8769 has an untrusted self-signed certificate. Check the successful endpoint first to establish reachability.

Before testing, save the exact bytes of the emulator's own `drives/c/resource/webkit/cacert.pem`, append only `ca.crt`, and restore the saved bytes in a `finally`/trap cleanup even when a test fails. Do not alter SDK/global trust or the release CA bundle. Keep generated keys, certificates and reports outside the repository. Never disable curl peer/hostname verification.

Load `https://localhost:8767/tls.html?autorun=1`. Require `PASS: tls 6/6` and six current report outcomes. Each negative WSS test requires its own error event, no open event, and closure; an HTTPS page failing to load cannot stand in for these checks. Inspect errors and server reachability to distinguish certificate rejection from unrelated connection failures.

The HTTPS endpoint also serves `webcrypto.html` for a secure context. Cryptographic standard vectors and API rejection checks are separate from network and TLS results.

## Automated emulator driver

After firmware and browser prerequisites are installed, start the fixture server above and use the same `XDG_*`, `DISPLAY`, `EKA2L1`, `SYMBIAN_SDK_PREFIX`, and `SYMBIAN_WEBKIT_WORK` environment used for the build and emulator installation:

```sh
python3 Tools/Symbian/tests/run-browser-network-tests.py /path/to/build/browser/e32/browser.exe \
  --reports /path/to/results --ca /path/to/new-test-tls/ca.crt
```

The Linux driver runs HTTP and TLS sequentially through `eka2l1/run-browser.sh`. `--suite network` runs only HTTP/WebSocket checks without changing trust; `--suite tls` runs only TLS. `--timeout` sets the bounded observation period per suite (180 seconds by default); an additional watchdog handles a stuck driver. Browser and JSC runs sharing the data directory are excluded while the suite holds its locks. A private process group and Linux child subreaper allow cleanup even if the driver's emulator starts a separate session.

Each page reports the unique `run` query token. The driver requires exactly one new matching report, all expected names and PASS statuses, the matching fresh console lines, and no panic/access violation in current guest/host logs. It saves a result manifest with the browser SHA256, report, log and screenshot paths. TLS temporarily adds only the provided public test root to this emulator's CA file; original bytes, permissions, timestamps, or the original absent-file state are restored even on failure or interruption.

Before launching the TLS page, the driver checks all three listeners against the `good.crt`, `wrong.crt`, and `untrusted.crt` files beside `--ca`. It verifies each endpoint's certificate fingerprint and `/ok` response, then requires the wrong-host connection to fail specifically with a hostname mismatch and the untrusted connection to fail specifically with an issuer/trust error under the test CA. A disposable host SSL context trusts the untrusted leaf only for its identity/reachability check; peer and hostname verification remain enabled. Ports, certificate fingerprints and verification reasons are saved in the manifest. An unavailable or misconfigured negative endpoint fails the preflight. Symlinked or hard-linked emulator CA bundles are rejected without modification.

Host-only runner regressions do not start EKA2L1:

```sh
python3 Tools/Symbian/tests/test-browser-network-runner.py -v
```

They cover trust rollback, report correlation, exclusive/inherited locks, orphaned-session cleanup and TLS listener checks, including a complete driver run with a fake emulator. Their success does not establish guest network or TLS behavior.
