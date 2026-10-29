# SIS registry length regression

`run-host.py` extracts the real registration function and uses the actual EKA
headers. It builds old/new controls and syntax-checks the complete fixed TU in
a separate directory. It does not modify the supplied EKA source or start EKA.

```sh
python3 Tools/Symbian/tests/eka-sis-metadata/run-host.py \
  --eka-root /workspace/EKA2L1-validation --work /tmp/eka-sis-metadata
```

The EKA checkout must already be configured so its build.ninja supplies the
actual production include flags. `cases.json` records compressed/uncompressed
lengths independently decoded from local SIS 0.3.41, plus three ordinary edge
cases. See `docs/validation-belle/installer-metadata/` for source provenance,
old-control failures and the real installation metadata failure.
