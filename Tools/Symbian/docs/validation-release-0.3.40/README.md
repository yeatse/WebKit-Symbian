# Release 0.3.40: downloaded CI packages installed and executed

[CI 40](https://github.com/yeatse/WebKit-Symbian/actions/runs/40925052880) built
`2.52.6-symbian` at `d46c6c5a84b9b96c12827be8214fbb35a93207b7`.
[Download the three SIS packages and SHA256SUMS](https://github.com/yeatse/WebKit-Symbian/actions/runs/40925052880/artifacts/12924582582).
Install the ICU data package first, then the browser and/or JSC package.

The downloaded ZIP digest, three package hashes, package UID/version/CRCs and
all 11 decompressed payload hashes passed [independent inspection](ci/README.md).
The actual extracted ICU data passed the host IDNA regression with its negative
controls. The build targets ARMv6; both E32 executables declare VFPv2.

The three downloaded **0.3.40** packages were then installed through the real
emulator SIS installer. Registry versions and controllers matched these SIS
files, and all 11 installed files matched their payloads before and after the
smoke runs. The executable/resource/data files were installed only by that
installer. The previous C-drive browser was moved aside; all browser checks
launched the installed `E:/sys/bin/browser.exe`.

| Exact-CI browser smoke | Result | Observation / closure |
| --- | --- | --- |
| IDNA / domain handling | 9/9 PASS | 30 seconds, normal host exit 0 |
| ARM11 DOM getters / mutations | 8/8 PASS | 30 seconds, normal host exit 0 |
| WebCrypto from this mainline commit | 19/19 PASS | 30 seconds, normal host exit 0 |

All three fresh runs had no guest panic or access violation. Their logs,
screenshots, launcher scripts and frozen mainline fixtures are in
[`runtime/`](runtime/smoke-result.json). The fixture hashes match the exact CI
source; the later Belle 29-check Crypto page was not used.

The tested browser SHA-256 is
`99ec5fe0d84c1c7ae18b06b0d2e175788d9989fee24ccd9fa3cef93bbe57570c`; JSC is
`d90bc977bdf6476dd32c9194c46eb9df5b2472217b04a9d75d150939a5862f0c`.
The host emulator is `5f8704eb…` with mapped-volume patch 0018; its complete
identity is in [`runtime/post-run-identities.json`](runtime/post-run-identities.json).
ICU remains `808fc9bd…` and the original CA bundle remains `a41b5d35…`.

This is an upgrade of the existing Nokia X7 validation profile. It verifies
these actual distribution packages and the three listed smoke suites. Full
media, network and memory-stress evidence has its own recorded browser/host
identities; those earlier results are not relabeled as CI 40 execution.
`preparation.json` preserves the earlier preparation-stage status and is
followed by the final PASS installation/smoke/post-run records. Previous raw
registry data, candidate backups, executables and SDK/firmware files are excluded.
