# Symbian filesystem regression tests on the host

Run `./run.sh` with a little-endian host C++20 compiler, iconv and a GNU-compatible linker. The fixture
compiles the actual `Source/WTF/wtf/symbian/FileSystemSymbian.cpp`. Small WTF
container/string adapters stand in for linking the ARM WTF library; no filesystem
or path implementation is copied into the fixture. The production
`SymbianFilePath.h` is included from `Source/WTF` rather than copied.

The tests use real host files, hard links and symbolic links. Linker wrappers
inject interrupted reads/writes, short writes, storage failure, cross-device
rename failure and permission errors. Drive letters are represented by `C:` and
`E:` directories under an automatically removed temporary directory.

Coverage includes drive/root/dot/parent paths, ancestor boundaries, no-overwrite
copy, preventing truncation through source/destination aliases, partial writes,
failed exclusive-copy cleanup, cross-device ordinary-file moves, unsupported
hard links, temporary directories and recursive deletion. Native volume results
are injected only at the RFs bridge boundary to test drive selection, relative
Open C paths, zero available space, values above 4 GiB and errors in the actual
WTF adapter. The separate [native probe](../native-volume/README.md) exercises
RFs itself; host injection is not evidence for a real Symbian volume.

This validates path and I/O control flow. It does not establish Symbian Open C
semantics, on-device Unicode conversion or filesystem permissions, and does not replace
guest testing.

The host fixture explicitly uses `-fshort-wchar` so the unchanged production
helper requires its real two-byte `wchar_t` contract. Its test-only String
adapter keeps UTF-16 code units as well as UTF-8 bytes. Test-only `wstat` and
`waccess` entry points receive those code units, use the host's independent
iconv conversion, then call real host stat/access. They never call the host
libc's four-byte-wchar_t wide interfaces. This is an explicit boundary adapter,
not ARM ABI, Open C or Symbian filesystem execution evidence.

Three fresh controlled files cover ASCII content, an empty file and a filename
containing both a BMP character and a surrogate pair. The real production
functions check exists, size, modification time and followed type; a symlink
checks that the no-follow path remains lstat. Empty, embedded-NUL and overlong
paths are rejected before either test-wide entry point; the exact 256-unit
boundary and NUL terminator are also checked through the actual helper.
