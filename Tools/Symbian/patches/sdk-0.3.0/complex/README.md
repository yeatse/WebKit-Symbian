# Complete LLVM double-complex helpers for SDK 0.3.0

Apply the complete SDK native stack through 0009, then reconstruct the original
SDK prefix in this order: allocator, math0007, complex0009. Do not run the old
whole-archive allocator guard over an already math/complex-modified prefix.
Only the pristine release prefix is cached; the complex work cache is separate
and the helper always rebuilds/verifies on cache hits.

```
python3 scripts/rebuild-sdk-complex.py --sdk SDK --source PLATFORM_SOURCE --output COMPLEX_WORK
```

The default stages three ARMv6 runtime archives and links actual std::complex
canaries. An explicit `--install` can replace the three archive files only,
with pinned backups and caught-exception rollback. This is not a cross-file
power-failure transaction. The helper does not change SDK headers or sources.

The manifest pins SDK0009's owner CMake postimage, the already installed
math0007 header/member, compiler version, profiles and the small original LLVM
source closure at the same revision selected by the SDK. Original Apache/LLVM
license and source notices remain. Every original archive member is preserved;
only muldc3.c.obj and divdc3.c.obj are appended, or rebuilt in place when an
existing complete pair matches the newly compiled objects exactly. Partial or
unknown existing pairs are refused before installation. Cached stage bytes are
always overwritten from the current prefix, never blindly installed.

Source, ELF/ABI format, positive function sizes, exact archive preservation
and a real ARM link are separate from loader/native numerical/audio acceptance.
The helper does not enable WebAudio, use fast-math, or manufacture libm proxies.
