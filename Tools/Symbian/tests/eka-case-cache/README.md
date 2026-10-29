# EKA case-folded lookup after file creation

The actual EKA directory-name cache uses a host modification stamp with second
precision. A negative lookup followed by a successful file creation in the same
second must not hide the new file from a differently cased lookup. This matters
to EFsrv's existing-file check before an exclusive create.

With a configured EKA host build and its normal compiler environment:

```sh
python3 Tools/Symbian/tests/eka-case-cache/run.py \
    --eka-source /path/to/EKA2L1 --eka-build /path/to/EKA2L1/build \
    --patch /path/to/0023-Invalidate-case-fold-cache-after-file-creation.patch \
    --output /new/regression-directory
```

The script reads the real CMake compiler command with `ninja -t commands`; it
does not run Ninja's build graph or launch EKA. It compiles the old pinned
`fileutils.cpp` and the patched source into private copies of the configured
`libcommon.a`. The executable exercises actual file creation and path lookup on
a case-sensitive host filesystem. No resolver implementation is copied into the
test, and the EKA source, build outputs and guest drives remain untouched.

Both `wb` and `ab` reproduce the old missing-alias result in the same real clock
second and pass after the patch. Exact-source bytes, explicit cache refresh,
failed-open behavior and ordinary existing-file writes are checked separately.
Two pre-existing host directories named `WebKit` and `webkit` retain distinct
inodes and contents in both variants. The patch does not merge, remove or repair
such an ambiguous host namespace; that condition must not be silently treated
as a valid Symbian directory layout.

The independent native copy probe still needs a fresh, unique guest fixture
directory and real EKA execution after the patch. Host cache checks alone do not
constitute native OpenC/O_EXCL acceptance.
