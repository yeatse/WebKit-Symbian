# ICU 0.3.38 package verification

The local hotfix package preserves UID `0xE1A5C013`, vendor `symbian-webkit` and
the one installed file `C:\resource\icu\icudt70l.dat`. Version 0.3.38 is newer
than the main-branch CI package 0.3.36 and the graphics precheck 0.3.37.

| Item | Bytes | SHA-256 |
| --- | ---: | --- |
| Local `symbian-webkit-icu-0.3.38.sis` | 4,768,936 | `0289b4c04856bc87381f48d9996dad2bcc114f49d998898cd4006b97c790164b` |
| Extracted `icudt70l.dat` | 10,261,824 | `808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d` |
| Previous 0.3.36 data | 10,202,528 | `5fe6ac8866742956f7fac9bcee571b6450cb56808445c19e544c08bdea66755f` |

`data-rebuild-proof.json` records the full ICU70 archive, scripts and resulting
data hashes. The saved `source-*.sh`/`.py` files are the exact build-script/filter
snapshots used. `rebuild-data.sh` records the original trim and locale-index
stage run in `/workspace/symbian-idna-work`, with existing host tools and the
full source archive read only. It did not rebuild target libraries or modify
`/workspace/symbian-work/icu/install`. Its absolute paths describe that recorded
execution; the maintained build entry remains `scripts/build-icu.sh`.

The regenerated package is byte-identical to adding the missing 59,264-byte
`uts46.nrm` item to the old data (59,296 bytes of archive growth including its
index/alignment). This preserves the existing locale selection.

`sis-verification.json` compares the actual downloaded CI36 ICU SIS with the
local hotfix: header and section CRCs, UID, version, install path, single-file
payload hash and decompressed size all pass. Both packages are unsigned; no CI
signing key was substituted. Binary SIS/ICU data and keys are not stored here.
The local package is at `/workspace/symbian-idna-work/sis/` in this workspace.

The standalone Python inspector can repeat those structural checks without EKA:

```sh
python3 verify-icu-sis.py /path/to/symbian-webkit-icu.sis --version 0.3.38 \
    --extract /tmp/idna-check/icudt70l.dat --output /tmp/idna-check/sis.json
```

The extraction path is explicit; the tool never writes to the package's guest
install destination. The original verification record remains unchanged.

`sis-payload-idna.json` tests the data extracted from the actual new SIS using
the maintained ICU IDNA regression. It links ICU70's real host implementation
with empty stub data, prohibits file fallback, and supplies the explicit data
bytes. ASCII/IDN/punycode/nontransitional/width cases pass, invalid ContextJ,
bidirectional and ACE cases are rejected, and all/en/zh filtered packages pass.
Deleting UTS46 from each variant reproduces `U_FILE_ACCESS_ERROR` as the negative
control. Existing browser A/B evidence lives in the parent directory; structural
SIS checks here are not an installer-upgrade execution claim.

The successful main CI38 is
[40908209410](https://github.com/yeatse/WebKit-Symbian/actions/runs/40908209410),
commit `17cf20e6748e87a1749f11b1bf018cf2304eb392`.
`ci38-artifacts.json` records the downloaded ZIP hash and all three nonempty
SIS hashes checked against its `SHA256SUMS`.
`ci38-icu-sis.json` verifies the actual CI ICU package's version 0.3.38, UID,
destination, CRCs and extracted payload. The entire CI ICU SIS is byte-identical
to the local hotfix above. Its extracted data contains `uts46.nrm` and passes
the independent real-ICU/negative-control suite in `ci38-payload-idna.json`.

[Download CI38 SIS packages](https://github.com/yeatse/WebKit-Symbian/actions/runs/40908209410/artifacts/12914250242)
or [ELF/debug inputs](https://github.com/yeatse/WebKit-Symbian/actions/runs/40908209410/artifacts/12913900992).
