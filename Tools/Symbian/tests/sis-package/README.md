# WebKit installation package

Builds `symbian-webkit.sis` with miniature build outputs and the project test
pages. Requires Python 3 and Bash.

```sh
python3 Tools/Symbian/tests/sis-package/run.py
```

The test checks the WebKit package name and UID, every installation path and
its payload bytes. The split executables with their DLLs, resources, ICU data,
CA certificates and fonts.conf use the selected drive; test scripts and pages
install on C:. Use `--icu-data static` to check a build with
ICU data linked into its executables.
