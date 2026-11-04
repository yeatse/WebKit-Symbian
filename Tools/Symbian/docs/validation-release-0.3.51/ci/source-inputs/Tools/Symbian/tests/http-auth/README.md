# Symbian HTTP authentication contracts

Run from the repository root with a host C++ compiler supporting ASan/UBSan:

```sh
python3 Tools/Symbian/tests/http-auth/run-native.py
python3 Tools/Symbian/tests/http-auth/run-frame.py
```

The first test compiles production `AuthenticationUI.cpp` with a small AVKON
adapter. It covers the native two-step flow, masked password resource selection,
accepted empty fields, Cancel at either step, cancellation for the correct
resource identifier, nested challenges, Leave, attempt bounds and container
release during a modal dialog. Allocation accounting and sanitizers check the
controller/dialog lifetime. It does not emulate rendering or the Symbian FEP.

The second test extracts the actual Legacy challenge method bodies into a frame
lifecycle harness. It covers Basic/Digest acceptance, explicit refusal to prompt
for 407/proxy/server trust/unsupported schemes, the three-prompt limit, canceled
results, load failure, frame detach, replaced DocumentLoader and a challenge
cleared by ResourceHandle while the callback is active. It checks that the
credential has `CredentialPersistence::None` and a nested request cannot take
over an active prompt.

On Belle FP2, manually verify the integrated browser against controlled HTTP
Basic and Digest endpoints: correct credentials load the response; wrong
credentials prompt at most three times; Cancel stops the request; blank password
is accepted for an account that uses one. The engine's conventional empty
Credential still means no credential when both username and password are blank.
Verify navigation, reload, Stop, parent/child frame removal and app exit while
either step is visible. Verify masked keyboard input and no retained password
on reopening the prompt. Repeat a redirect to a different origin (including a
port change) and check that no previous Authorization is sent. An invalid HTTPS
certificate must fail before any authentication prompt. A 407 response must not
show the origin username/password UI; interactive proxy authentication is not
implemented by this bridge.

Host contracts and ARM syntax/resource checks do not substitute for those native
UI/network integration checks. No emulator or device execution is claimed here.
