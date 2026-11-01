# Classic WebAudio math correction

2026-11-01. The missing name was a port visibility problem; normal ARM function
contract checks also found an existing zero-byte implementation. Root has now
applied the reviewed three-file combined patch. This agent performed only
external source-overlay builds and normal compile/link/numerical checks; no
shared SDK source/prefix, guest, emulator assets, display or server was changed.

`webaudio-math-build-fix.patch` contains `std-log2.patch` + `math-owner.patch`.
Its exact identity and before/after source hashes are in `result.json` and
`source-identities.json`. Reverse apply-check passes against Root-applied source.

## Source and real APIs

Open C declares `log`, `logf`, `exp2`, `exp2f`, `powf`, `sinf`, `sqrtf` and
`atan2f`, but not `log2`/`log2f`; its actual libm proxy has no `log2f` export.
WebKit already owns those C99 functions in `WTF/wtf/symbian/LibcxxSupport.cpp`.
Two WebCore sources now include `<cmath>` and use its float `std::log2`
overload, which lowers to owned C `log2f`. The integer `periodicWaveSize()`
argument is explicitly converted to float to retain the original input
conversion. No public header/SDK edit or unnecessary whole-dependency rebuild.

Normal symbol/section-size checks found actual existing owner `log2f` defined
with zero bytes, while `log2` was 40 bytes. A float `std::log2` ELF canary linked
that zero-byte symbol without a libm `log` dependency. Symbol presence alone was
insufficient. The existing float function forwarded through builtin-recognized
`log2`; the repair directly calls real double `log` with the same log2(e)
factor, avoiding folding the float/double forwarding back into `log2f` itself.
The actual complete owner TU now defines `log2f` at 56 bytes and imports real
libm `log`; `log2` remains 40 bytes. No fake export or replacement libm.

## Checks and evidence limits

* Actual failing engine unified TUs 53/54 compile exit 0 with exact current ARM
  VFPv2 softfp commands plus source VFS overlay. Their three existing warnings
  remain. Output/dependency destinations are external to shared build.
* Complete real `LibcxxSupport.cpp` owner TU compiles exit 0 with exact engine
  command. Nonzero function-size and real-import gates pass; no disassembly or
  runtime diagnostics were used.
* Actual ARM ELF canary statically proves float overload type and links the
  newly compiled real owner object, original unchanged libWTF, real Streams
  archive and native libm/libc/euser/pthread proxies. Nonzero `log2f` plus real
  libm `log` import pass. `link-result.json` contains exact commands and hashes.
  The thin libWTF archive header stayed at
  `045b0ebab6a9888a6649f10ab113c7936ee681ed23a83472b6ce3c57f0c59f33` during
  these tests; that is metadata, not a digest of every thin-archive member.
  The explicit complete fixed owner object is separately pinned in
  `owner-object-identities.json`; baseline source is reconstructed against
  its before-SHA and separately compiled to retain both size contracts.
  Root's subsequent normal archive rebuild is a distinct step.
* Extracted actual float body, renamed only to avoid replacing host libc, is
  tested under GCC -O2, GTest, ASan/UBSan: 65,534 fixed positive finite float
  patterns (every exponent, 257 mantissas), at most 1 float ULP against host
  `std::log2`; ten domain/special checks cover 1/2/.5, both zeros, infinities,
  negative input and quiet NaN. Two GTests / CTest 1/1 pass. This does not prove
  actual Belle libm numerical behavior.

Earlier failures remain: canary first omitted real pthread proxy, then an import
gate rejected the zero-byte implementation. Host configure initially omitted
GTest_DIR; the no-tests output is unsuccessful evidence. Final configure uses
installed GTest and `ctest --no-tests=error`.

Selected remaining WebAudio float sqrtf/powf/sinf/exp2f/atan2f and double exp2
have real Open C declarations. The actual failing TUs show no other math
interface failure; this is not a complete libm audit. No loader, guest,
physical ARM11 or native audio acceptance is claimed.
