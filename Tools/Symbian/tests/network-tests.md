# Browser networking

Checks browser HTTP requests, redirects, cookies, cross-origin requests and TLS
certificate validation. Requires OpenSSL and the EKA2L1 setup in
[Testing](../docs/TESTING.md).

```sh
bash Tools/Symbian/tests/make-test-certificates.sh /tmp/network-tls
python3 Tools/Symbian/tests/serve-network-tests.py \
  --tls-dir /tmp/network-tls --reports /tmp/network-results
```

Run the browser suite in another terminal:

```sh
python3 Tools/Symbian/tests/run-browser-network-tests.py /path/to/browser.exe \
  --reports /tmp/network-results --ca /tmp/network-tls/ca.crt
```

The runner collects the page reports and browser logs. Passing results receive
expected HTTP data, enforce origin rules and accept or reject TLS certificates
according to the test CA and requested hostname.
