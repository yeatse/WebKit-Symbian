# Symbian filesystem regression tests on the host

Run `./run.sh` with a host C++20 compiler and GNU-compatible linker. The fixture
compiles the actual `Source/WTF/wtf/symbian/FileSystemSymbian.cpp`. Small WTF
container/string adapters stand in for linking the ARM WTF library; no filesystem
or path implementation is copied into the fixture.

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
semantics, Unicode conversion or filesystem permissions, and does not replace
guest testing.
