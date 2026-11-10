# SIS application metadata

Checks EKA2L1's SIS metadata parsing and application-registration information.
Requires an EKA2L1 checkout and a host C++ compiler.

```sh
python3 Tools/Symbian/tests/eka-sis-metadata/run-host.py \
  --eka-root /path/to/EKA2L1 --work /tmp/sis-metadata-tests
```

The runner builds the parser fixture and runs `cases.json`. It exits successfully
when the parsed package and application metadata match the fixture records.
