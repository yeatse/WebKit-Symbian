# CI 39 package verification

[GitHub Actions run 40921503443](https://github.com/yeatse/WebKit-Symbian/actions/runs/40921503443) succeeded for `graphics-media-symbian` at `defd29c69bda8781b42fec9ca4769f11a3ec15ed`. The packages have version **0.3.39**.

- [Three SIS packages and SHA256SUMS](https://github.com/yeatse/WebKit-Symbian/actions/runs/40921503443/artifacts/12919287469)
- [ELF artifact](https://github.com/yeatse/WebKit-Symbian/actions/runs/40921503443/artifacts/12919442127) (API metadata checked; this archive was not downloaded)

The downloaded SIS ZIP matches GitHub's SHA-256 digest. All three nonempty SIS files match `SHA256SUMS`; the package headers, section CRCs, UIDs, versions, 11 install paths and all 11 decompressed payload hashes passed independent inspection. These packages are unsigned, matching this workflow's configuration. JSC and browser declare VFPv2 (`0x12100028` E32 flags). The ICU payload is the repaired `808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d` data, and the actual extracted data passed the host IDNA regression, including missing-UTS46 negative controls.

The actual job log records the rebuilt SDK runtime SHA-256 `726c69c4798dab91f6e6957ef340107754bde4901127732571338afbe2007e26`, exactly 21 replaced and 82 unchanged archive objects, the unchanged 64 MiB reservation budget, and 13 passing runtime checks including the nested-checkout patch regression. The archive was installed before the JSC/WebCore/browser builds. `job.log.gz` preserves the complete job log; `verification.json` records its uncompressed SHA-256.

**Runtime evidence is a different binary.** The CI browser payload is `1c6ed85bb7d6cf775d9b9ee969c7b59183cc370f851b9529c76c38853438371c`; the locally installed and tested browser is the separately built `2db5` candidate. No CI 39 SIS package was installed or executed for this check. Comparing the CI commit with the local checkout found no differences in production `Source`, top-level CMake, browser sources, Symbian CMake/build scripts or workflow. Subsequent validation commits and EKA patch 0017 document/test the local candidate and fix emulator audio read-ahead; they do not establish byte identity with the CI binaries or constitute a guest run of those binaries.

`package-checksums.json` contains SIS sizes and SHA-256 values; `sis-payload-verification.json` contains all 11 payload hashes and CRC results. `icu-sis-verification.json` and `icu-payload-idna.json` record the extracted ICU checks. Absolute workspace paths in those reports identify the original verification inputs and are historical provenance, not required checkout paths. No SIS, ELF, SDK archive, signing key or firmware is included here.
