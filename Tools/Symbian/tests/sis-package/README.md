# WebKit installation package

Builds `symbian-webkit.sis` with miniature build outputs and the project test
pages. Requires Python 3 and Bash.

```sh
python3 Tools/Symbian/tests/sis-package/run.py
```

The test checks the WebKit package name and UID, all 13 installation paths and
their payload bytes. Executables and registration resources use the selected
drive; shared data installs on C:. Use `--icu-data static` to check a build with
ICU data linked into its executables.
