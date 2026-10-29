# Local SIS 0.3.41 package verification

The three local packages passed independent structural and payload verification.
This records packaging only: these packages were not installed or guest-tested
by this check, and this is not a GitHub Actions run 41 result.

| Package | Package UID | Version | Payloads | Bytes |
| --- | --- | --- | --- | --- |
| `browser.sis` | `0xe1a5c012` | `0.3.41` | 7 | 21,966,284 |
| `jsc.sis` | `0xe1a5c011` | `0.3.41` | 5 | 6,895,152 |
| `symbian-webkit-icu.sis` | `0xe1a5c013` | `0.3.41` | 1 | 4,768,936 |

All 13 decompressed payloads match their pinned source files byte for byte.
The checks cover SIS UID checksums, controller/data CRCs, package UID/version,
install destinations and order, sizes and SHA-1 metadata, and source SHA-256.
Both executables also pass E32 UID/header CRC, SID, capability and VFPv2 checks.
Browser capabilities remain `0x2000`; JSC capabilities remain `0x0`.
All three packages are unsigned (zero signature chains).

The browser is candidate 10, SHA-256
`3a2385c1bd571d51c98f58f967a8ecf499ba1dcfb86d42d58b1e7dc82dd70317`.
The reused JSC is
`d26cf9cad3c4e47a0d3a559598bd16ba2bfad772dabeb21c17cb8eb211b44bcc`.
The ICU data is the UTS46-fixed
`808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d`.
The 13-payload plan includes the new `browser_ui.rsc` and `fonts.conf`;
the older CI40 11-payload evidence remains unchanged.

`package-verification.json` records full package/payload hashes, source paths,
identities and checker hashes. `SHA256SUMS` identifies the actual SIS bytes.
`input-plan.json` is the exact plan used. The original candidate-2 plan is
preserved externally at `/workspace/symbian-font-work/belle-payload-plan-candidate2.json`.
Only verification metadata is stored here, not SIS or executable payloads.

From the repository root, with the recorded inputs still present:

```sh
python3 Tools/Symbian/scripts/verify-sis-payloads.py \
  --sis-dir /workspace/symbian-belle-work/release-0.3.41/sis \
  --plan Tools/Symbian/docs/validation-belle/package-0.3.41/input-plan.json \
  --version 0.3.41 \
  --output-dir /workspace/symbian-belle-work/release-0.3.41/verification-replay
```

The verifier accepts a version and source plan for future package checks; paths
must be updated explicitly if inputs are relocated. It imports the existing SIS
field reader from `validation-idna/package/verify-icu-sis.py`. Two negative
checks also passed: a plan requesting 0.3.42 rejected the actual 0.3.41 SIS, and
an incorrect pinned source hash was rejected. See `negative-checks.json`.
Ruff lint and formatting checks passed for the new verifier.
