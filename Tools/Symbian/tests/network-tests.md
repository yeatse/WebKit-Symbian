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
