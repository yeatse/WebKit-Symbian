# SameSite cookies

Checks SameSite classification and cookie attachment for navigation and request
contexts. The host suite compiles the production policy helpers.

```sh
python3 Tools/Symbian/tests/cookie-samesite/test-host.py --cxx c++
```

For the browser flow, create certificates and start the fixture server:

```sh
bash Tools/Symbian/tests/cookie-samesite/make-certificates.sh /tmp/samesite-tls
python3 Tools/Symbian/tests/cookie-samesite/server.py \
  --tls /tmp/samesite-tls --output /tmp/samesite-results
```

Follow the server's page URL in the browser with the test CA installed. The page
uses localhost and numeric-address origins and reports which cookies reach each
request. Passing results match the expected SameSite policy.
