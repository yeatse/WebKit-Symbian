# Native filesystem helpers

Checks the production Symbian path conversion and file operations, including
opening, copying, moving and metadata. Requires a C++20 compiler and iconv.

```sh
bash Tools/Symbian/tests/native-filesystem/run.sh
```

The runner builds the filesystem fixture and checks file contents and returned
paths. It exits successfully when all operations produce the expected results.
