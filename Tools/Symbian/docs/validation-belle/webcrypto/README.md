# Belle batch 1: WebCrypto guest verification

The candidate2 browser passed **29/29 checks** on EKA2L1 on 2026-10-29.
This is an emulator result, not a physical-phone result or a complete WebCrypto
conformance claim. [cases.json](cases.json) lists all 29 unique outcomes, extracted
from the exact current-run URL in [log.txt](log.txt); the fixture hash matches the
repository's `Tools/Symbian/tests/webcrypto.html` at validation time.

The cases cover SHA-1/256/384/512, HMAC, HKDF/PBKDF2, AES-CBC, AES-CTR key widths
and counter boundaries, AES-GCM authentication/tag rejection, AES-KW integrity,
key length/usage validation, and unsupported-algorithm rejection. In particular,
the PASS entries for RSA-PSS, ECDSA, Ed25519, X25519 and SHA-224 mean these
algorithms are **not registered and reject correctly**; they do not demonstrate
implementation. AES-CFB-8 likewise passes by rejecting its deprecated API.

The independent strict wrapper required the current URL's unique
`WEBCRYPTO DONE PASS 29/29 passed; 0 failed` marker, exactly 29 individual PASS
lines, no FAIL or guest panic/access violation, a screenshot, and driver exit 0
after normal WM closure. A separate archive check confirmed all 29 names are
unique. The CA bundle was unchanged before/after the run. The original wrapper
is retained byte-for-byte as `runtime-wrapper.py.txt`; its absolute workspace
paths describe the historical run and are not portable launch configuration.

Artifacts:

- [result.json](result.json): original runtime result and executable identities.
- [manifest.json](manifest.json): hashes for the copied evidence.
- [log.txt](log.txt), [emulator.log](emulator.log), [driver.log](driver.log): raw logs.
- [browser.png](browser.png): captured final browser view.
- [cases.json](cases.json): observed outcomes with unsupported-case meaning.

Browser SHA-256:
`91cf3692692f07b39ab495d32c6a4efa8270a66918ada79c681b2dbb446d0168`.
Emulator SHA-256:
`12c275e7f7b3fa8da4fa58ef4d048799db278baaeca7a45d3f78debad3051a4c`.
Fixture SHA-256:
`663277d8d541d0be51fa60d28af5bde763730fee8abc36417f6902710c3e4ffb`.

The earlier production AES-CTR overflow regression has separate
[before/after evidence](../aes-ctr-counter/README.md). The 29-case guest run adds
integration coverage; it does not replace the counter arithmetic regression.
