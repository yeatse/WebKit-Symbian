Run against the ICU 70.1 source/host build created by `build-icu.sh`:

```sh
python3 Tools/Symbian/tests/icu-idna/run.py \
  --icu-root /path/to/work/icu \
  --existing-data /path/to/release/icudt70l.dat \
  --output /tmp/idna-regression.json
```

The host needs a C++17 compiler (`CXX` is supported), Python 3, and the ICU host
`icupkg` and `libicuuc.a`. The runner compiles ICU's actual empty `stubdata.cpp`;
it never links the full host `libicudata.a`. The probe disables ICU file loading
with `UDATA_NO_FILES` and explicitly registers the selected package's bytes.
Every invocation uses a fresh process so cached normalizers cannot hide missing
data. The shared ICU build and installed data are never modified.

The test runs the production filter for all languages, English only, and Chinese
only, builds actual packages, and tests nine UTS #46 conversions/rejections with
the same options as WTF's `URLParser`. Removing `uts46.nrm` from each package
must reproduce `U_FILE_ACCESS_ERROR`; these negative controls also catch an
accidental fallback to complete embedded/system data. The locale indexes are
not rewritten in this IDNA-only test; the normal build retains that separate
step, and IDNA normalization is independent of locale resource indexes.

To reproduce an old broken package, add `--expect-existing-missing`. Do not use
that option to validate a release package. An unexpected success or error makes
the runner fail. No option changes the expectations for the newly filtered
packages or their negative controls.

For the actual browser, open [idna.html](../idna.html) as a local file. It runs
automatically, makes no network requests, and must report `IDNA RESULT 9/9 PASS`
in the page and console. This exercises the browser's URL implementation;
the host test exercises real ICU and packaging, not the full browser or Symbian
file installation. Neither test claims DNS, HTTP, TLS, or live-site coverage.
