# Local C10 SIS 0.3.41 formal installation

**PASS:** the three unchanged local candidate-10 (C10) packages were actually
installed with EKA `--install` on RM-707, in ICU, JSC, browser order. All 13
installed payloads match their pinned package sources byte for byte and by
SHA-256. All 13 registry sizes equal the uncompressed SIS payload lengths.
This is local package installation evidence, not a GitHub Actions run 41
artifact or result, and not a new browser or JSC application runtime test.

The host includes patches 0025 and 0026, has 65,474,624 bytes, and SHA-256
`0d2714494b322684872c0fc036c2f206b8514e54d021dc2ec1e3182af5855b98`.
[Host identities](host-identities.json) also record the four restored native
MMF asset hashes. No host or MMF binary is included in this archive.

| SIS package | SHA-256 | Payloads | Host exit |
| --- | --- | --- | --- |
| `symbian-webkit-icu.sis` | `5a2ad94da2911a9d586466dac58f40dd61af14c43c73a6613f44ed77a3e36692` | 1 | 0 |
| `jsc.sis` | `b32d1d49befbf977c591bb25a830688cd04f3676553690432e24adcd6d3265ad` | 5 | 0 |
| `browser.sis` | `15ea260aadeb1b8b75d9f47db26c78ad4a82d89035bd21ab935dfd9d71787710` | 7 | 0 |

Each installer reached its registration/completion marker, received normal
WM close, and exited with code 0. No forced cleanup occurred. No panic or access
violation matching the checker's patterns was observed. The five `!:` targets
resolved to E; eight fixed C targets remained on C. All three registries report
version 0.3.41, selected-drive mask 16 and drive mask 20. UID, target, index,
operation, options, SID, SHA-1 and SIS-controller identity also passed.
Registry trust fields are observations, not signature validation: the packages
are unsigned according to the separate package check.

The [acceptance summary](acceptance-summary.json) records host and SIS hashes
before and after, all 13 installed payload hashes and registry metadata, and
the normal exit results. Its comparison with the
[pre-install identities](preinstall-identities.json) confirms all eight
existing fixed C payloads and all five existing C shadows have unchanged sizes
and SHA-256 hashes. This preservation check covers those 13 recorded C files,
not a complete integrity check of the C drive. Existing `WebKit`/`webkit`
directories were preserved; the checker follows EKA's exact-spelling-first
path lookup. No payload was copied directly into a guest drive or substituted
into a C shadow. Both runtime locks were available and no EKA host process
remained after this check.

The [full result](result.json) is copied unchanged from the original run. The
[checker snapshot](install-verify-sis.py) has the exact hash recorded in that
result. It defaults to preparation only, and execution requires a pinned host
hash, both runtime locks, a fresh output directory and both environment scripts.
It can retain compressed-size observations for diagnosis, but only correct
uncompressed sizes yield metadata PASS. This archived script is an evidence
snapshot with original workspace paths, not a relocated standalone runner.

The completed command was:

```sh
. /workspace/eka-validation/env.sh
. /workspace/symbian-media-work/env.sh
python3 /workspace/symbian-belle-work/release-0.3.41/install-verify-sis.py \
  --execute --emulator-sha256 0d2714494b322684872c0fc036c2f206b8514e54d021dc2ec1e3182af5855b98 \
  --output-dir /workspace/symbian-belle-work/release-0.3.41/install-after-0026
```

The previous host's [run-4 result](previous-host/result.json) remains **FAIL**:
13 installed payloads passed and all three hosts exited normally, but all 13
registry file sizes contained compressed lengths. Its old host SHA-256 is
`3c4e27c486b696171502ff28b5577bbed48666c238c2ff85b335d79a0083467c`.
The new installation supersedes that metadata failure without altering the
historical report or the SIS packages. No old-host crash was reproduced or
diagnosed for this check.

`logs/*.log.gz` contains the new run's three original logs;
`previous-host/*.log.gz` contains the three original run-4 logs. Each gzip
expands to the exact source bytes, without filtering or normalization.
[manifest.json](manifest.json) records compressed-file hashes, original log
sizes and hashes, and metadata identities. Absolute paths in the unchanged
reports identify the original workspace observations; the manifest maps each
archived file to its source. Only scoped reports, checker source and installation
logs are archived; guest databases and unrelated private files are excluded.
