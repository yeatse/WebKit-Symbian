# CI 40 package verification

[Run 40925052880](https://github.com/yeatse/WebKit-Symbian/actions/runs/40925052880)
succeeded for `2.52.6-symbian` at
`d46c6c5a84b9b96c12827be8214fbb35a93207b7`. All three packages are version
**0.3.40**.

- [Three SIS packages and SHA256SUMS](https://github.com/yeatse/WebKit-Symbian/actions/runs/40925052880/artifacts/12924582582)
- [ELF artifact](https://github.com/yeatse/WebKit-Symbian/actions/runs/40925052880/artifacts/12924004026) (metadata checked; not downloaded)

The 33,509,419-byte ZIP matches GitHub's SHA-256 digest. All three SIS files match
`SHA256SUMS`. Package UID/header and section CRCs, version, all 11 install paths
and all decompressed payload hashes passed inspection. JSC/browser executable
UIDs are distinct from their package UIDs, as declared by the source build
configuration; both executables declare VFPv2. The packages are unsigned.
The four installed JS files and local HTML page match the exact CI commit,
and the CA payload matches the pinned release bundle.

The actual extracted ICU data is `808fc9bd…`, and passed eight independent host
IDNA test groups, including missing-UTS46 negative controls. The full decoded
job log records SDK runtime `726c69c4…`, 21 replaced and 82 unchanged archive
objects, the retained 64 MiB reservation budget and 13 runtime host checks.
`job.log.gz` preserves the exact connector-decoded UTF-8 job log and its SHA-256
is in `job-log-verification.json`. Direct access to GitHub's blob redirect was
forbidden in this environment; downloading the same official artifact through
the GitHub connector succeeded and retained the exact ZIP digest.

The CI browser payload is `99ec5fe0d84c1c7ae18b06b0d2e175788d9989fee24ccd9fa3cef93bbe57570c`.
It is not byte-identical to earlier locally tested `2db5` or the newer Belle
candidates. Installation and runtime evidence for this exact CI payload is
recorded separately in the release directory. No earlier binary's runtime
results are assigned to these packages.

To repeat the payload inspection, place the downloaded ZIP's four files in
`ARTIFACT_DIR/sis/`, then run:

```sh
python3 Tools/Symbian/docs/validation-release-0.3.40/ci/verify-payloads.py ARTIFACT_DIR
```

This writes verification JSON and extracted payloads outside the repository.
No SIS, ELF, SDK archive or private signing material is stored in this evidence
directory. Absolute paths inside reports identify the original inputs.
