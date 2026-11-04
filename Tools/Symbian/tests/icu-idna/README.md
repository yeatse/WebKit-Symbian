# ICU data and IDNA

Checks internationalized domain names and the Unicode/Intl data used by the
browser. Requires the built host ICU tools and the deployed ICU data package.

```sh
python3 Tools/Symbian/tests/icu-idna/run.py \
  --icu-root /path/to/work/icu --existing-data /path/to/icudt70l.dat \
  --output /tmp/idna-results.json
```

The runner checks the generated and supplied data packages. A passing result
confirms the required IDNA, normalization and locale data are available.
