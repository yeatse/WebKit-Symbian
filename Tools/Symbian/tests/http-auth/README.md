# HTTP authentication

Checks the native username/password dialog and its request/frame lifecycle.
Requires a host C++ compiler supporting AddressSanitizer and UBSan.

```sh
python3 Tools/Symbian/tests/http-auth/run-native.py
python3 Tools/Symbian/tests/http-auth/run-frame.py
```

Passing checks accept Basic/Digest credentials, propagate Cancel and release the
prompt when the request or frame closes.

For the integrated browser flow, open a controlled Basic or Digest endpoint,
enter valid credentials and verify its response. Repeat with Cancel and check
that the load stops. Password input uses the native masked field.
