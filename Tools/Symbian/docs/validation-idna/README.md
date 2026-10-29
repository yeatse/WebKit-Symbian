The reported phone crash at `URLParser.cpp:3001`,
`internationalDomainNameTranscoder`, is the explicit failure path after
`uidna_openUTS46`. ICU 70.1's `UTS46` constructor requests
`Normalizer2::getInstance(nullptr, "uts46", UNORM2_COMPOSE, error)`.
The previous Symbian data filter explicitly removed `uts46.nrm`, so initializing
the transcoder failed even though the rest of ICU's data package was installed.
The phone log reports loading `baidu.com` before the crash; it does not identify
which later document, URL, or subresource first required IDNA conversion.

The source fix retains `uts46.nrm` for every locale selection. UTS #46 is used
by the browser for domain names, independently of the user's language or the
Intl locales kept in the package. Legacy StringPrep `.spp` files belong to a
different API and remain filtered; no IDNA checks or URL error handling are
disabled.

`single-item-comparison.json` records a controlled test of the existing built
package: it returned `U_FILE_ACCESS_ERROR (4)` and a null UIDNA pointer. Adding
only the 59,264-byte `uts46.nrm` item from the matching ICU 70.1 full package
changed the result to `U_ZERO_ERROR` and a non-null pointer. Both tests used the
same executable, linked to real ICU70 `libicuuc.a` plus empty stubdata, with
`UDATA_NO_FILES` and explicit `udata_setCommonData`. No complete embedded data
or fallback data directory could supply the missing item. The hashes identify
the build artifacts tested here, not an independently extracted phone file.

`host-regression.json` records the reproducible
[packaging/IDNA test](../../tests/icu-idna/README.md). The full package and the
production filter's all-language, English-only, and Chinese-only outputs pass
nine cases each: ASCII, Latin and Chinese IDNs, punycode, nontransitional sharp s,
width/case mapping, and rejection of invalid ContextJ, bidi, and ACE labels.
Each filtered package with UTS46 removed fails with the expected error 4 in a
fresh process. The old installed build package reproduces the same failure.

Reproduction used:

```sh
python3 Tools/Symbian/tests/icu-idna/run.py \
  --icu-root /workspace/symbian-work/icu \
  --existing-data /workspace/symbian-icu-idna-regression/original/icudt70l.dat \
  --expect-existing-missing \
  --output /workspace/symbian-icu-idna-regression/host-regression.json
```

The regression leaves shared ICU source/build/install data unchanged. It proves
the data dependency and the packaging fix on the host.

The bounded local [browser page](../../tests/idna.html) also passed a real
Symbian guest A/B with the same browser and emulator binaries. With the old
package, the ASCII test passed, then the first international domain reproduced
`URLParser.cpp:3001` / `internationalDomainNameTranscoder` / `USER-EXEC 3`.
With the repaired package, all nine cases passed and the 20-second run closed
normally with host exit 0 and no panic. The fixture performs no network
requests. [guest-result.json](guest-result.json) records binary/data hashes and
the raw run locations; [guest-old.log](guest-old.log) and
[guest-repaired.log](guest-repaired.log) retain the results. The guest's original
data file was backed up and restored after the comparison.

The subsequent [SIS installation check](guest-sis-install.json) installed the
actual 0.3.38 package, checked the registered UID/version and controller, and
verified the installed data bytes. The browser then passed the same nine cases
without the runner copying ICU data into the guest. The repaired data remains
installed after this check. Host exit was 0 with no guest panic.

[Package verification](package/README.md) records the successful main-branch
CI38 and downloaded artifacts. Its ICU SIS is byte-identical to the locally
installed hotfix. These checks establish the packaging and EKA2L1 behavior;
another real-phone run remains outstanding.
