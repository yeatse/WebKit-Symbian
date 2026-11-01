# Actual WTF file worker

`worker.cpp` calls the current production WTF API and initializes its main thread
with `WTF::initializeMainThread()`. It requires no WebCore, JavaScriptCore or GUI.
The parent owns CMake, linking, packaging, the preserved runner and validation.
This source is prepared for compile-only checking; no guest run is claimed.

Launch with `--run TOKEN` (8–64 ASCII letters, digits, `_` or `-`). The runner must
create only its fresh `C:\Data\WebKit\wtf-file-owner-TOKEN` directory with:

| index | filename | bytes | SHA-256 |
| --- | --- | ---: | --- |
| 0 | alpha.bin | `bytes(range(256))*4` (1024) | 785b0751fc2c53dc14a4ce3d800e69ef9ce1009eb327ccf458afe09c242c26c9 |
| 1 | zero.txt | empty (0) | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| 2 | 字😀.dat | UTF-8 `Belle 文件上传 😀\n` (24) | 8a4a90e74c3d9c754d0f3ab6b391646f7de56516d70a5ad20c05e49030a481da |

Set all three mtimes to Unix 1700000000; WTF's returned time must be finite and
within two seconds. Paths use real UTF-16 String construction, preserving the
supplementary character's surrogate pair. No fixture is written by the worker.

The owned `C:/Data/WebKit/wtf-file-owner-contract.log` has exactly seven lines:
BEGIN JSON (`suite`, `version:1`, `run`), ABI JSON, three CASE JSON records in index
order, RESULT JSON (same identity, `passed`, `failed`, `total:3`), then the normal
exit text. CASE fields are `api:"WTF::FileSystem"`, `file`, `pass`, `exists`,
`exists_errno`, `size_available`, `size`, `size_errno`, `mtime_available`, `mtime`,
`mtime_ok`, `mtime_errno`, `opened`, `open_errno`, `read_ok`, `read_errno`, `eof`,
`bounded`, `bytes`, `exact`, `hash_ok`, `sha256`. Unavailable size is 0 with its
availability flag false; unavailable/nonfinite mtime prints -1 and fails. Reads
use actual FileHandle::read until EOF, bounded to 2048 bytes. Empty files must
return EOF and the empty SHA. CASE is reported after the real FileHandle RAII
destructor; no close status is fabricated. Pass requires every API and exact byte,
hash, size and time check. Exit is 0 on three passes, 1 on a case failure, or 2 on
invalid launch/log setup. No paths, enumeration or unknown filenames are logged.
