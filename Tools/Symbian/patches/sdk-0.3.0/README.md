# SDK 0.3.0 corrections

## Linux resource compiler identifiers

The [rcomp correction](rcomp/README.md) fixes crashes on valid enum aliases and
symbolic resource links after preprocessing without line directives. Its pinned
rebuild script validates the original SDK tools, builds the host compiler and
runs real resource-compilation regressions before replacing only `bin/rcomp`.
It changes no SDK runtime library or resource layout.

## 32-bit mimalloc reservations

`0001-mimalloc-32bit-pagemap.patch` changes only the target's 32-bit mimalloc
configuration. The 64 MiB native RChunk reservation budget is unchanged.
Upstream's page map avoids the default 32 MiB arena-alignment slack. In the
real EKA pressure probe, the shipped runtime and a rebuilt negative control
both reached 15.75 MiB of live 256 KiB objects; the patched runtime reached
54.5 MiB, with contents, alignment, release/reuse and over-budget rejection
checks passing. This is separate from WebKit's aligned JSC-block pool.

From the checkout root:

```sh
python3 Tools/Symbian/scripts/rebuild-sdk-runtime.py --sdk "$SYMBIAN_SDK_PREFIX"
# After reviewing the verification.json result, install the checked output:
python3 Tools/Symbian/scripts/rebuild-sdk-runtime.py --sdk "$SYMBIAN_SDK_PREFIX" --install
```

The script uses `$SYMBIAN_WEBKIT_WORK/sdk-runtime` (or `--work`), downloads only
fixed Git revisions, verifies every input source/header and the exact release
archive hash, and rebuilds 20 mimalloc C objects plus the SDK primitive. It
accepts only the ARMv6 default runtime from SDK 0.3.0. The other 82 members,
including the native RChunk implementation and its fixed budget, remain byte
identical. The manifest pins the resulting deterministic archive hash too.
Shared public symbols, object/TLS sizes, strong/weak external dependencies and
ARM ABI attributes must match; only three named internal page-map symbols and
the optimization-goal attribute may differ. A mismatched SDK/library causes
an error. Default execution writes a separate output; `--install` updates only
the default ARMv6 archive and its host-side mimalloc configuration header.
Other architecture/runtime profiles are untouched.

CI saves the original SDK cache before modification and keeps the runtime
source/output cache separate. It rebuilds and verifies even on cache hits,
then runs real-archive rejection fixtures. `verification.json` records all
member hashes, compiler/source identity and commands. Guest evidence and its
limits are in [validation-memory/sdk-mimalloc](../../docs/validation-memory/sdk-mimalloc/README.md).

Patch application uses an explicit independent Git root inside the build
directory and ignores inherited Git worktree/index/configuration environment.
It checks the patched header's fixed hash as well as the command exit status.
The regression suite includes a real nested Git checkout and polluted outer
Git environment; a full rebuild from the release archive is also recorded
with its work directory inside the WebKit checkout.

## Missing Symbian^3 headers

The 0.3.0 SDK's Video inventory selected `videoplayer.h` and `videorecorder.h`, omitting the graphics-surface extension headers. These files are unmodified upstream headers, including their original copyright and Eclipse Public License notices, from the exact revisions already recorded in the SDK's `share/symbian/native/sources.json`:

- [videoplayer2.h](https://github.com/SymbianSource/oss.FCL.sf.os.mmaudio/blob/a39c075cce88671b23d13598fda2f7cbf008f3bb/mmlibs/mmfw/inc/videoplayer2.h) and [surfaceeventhandler.h](https://github.com/SymbianSource/oss.FCL.sf.os.mmaudio/blob/a39c075cce88671b23d13598fda2f7cbf008f3bb/mmlibs/mmfw/inc/surfaceeventhandler.h), from Symbian OS Multimedia Audio.
- [graphics/surface.h](https://github.com/SymbianSource/oss.FCL.sf.os.graphics/blob/ff133bc50e6158bfb08cc093b0f0055321dcde99/graphicsutils/commongraphicsheaders/inc/surface.h), from Symbian OS Graphics.

`manifest.json` records the source repository, revision, path, Git blob SHA-1 and file SHA-256 for each header. `scripts/apply-sdk-patches.py` validates the SDK version, source revisions, hashes and all existing destinations before adding missing files. Reapplication is harmless; it refuses to overwrite a different file. It performs no network access and changes no existing SDK ABI, library or proxy.

The existing `mediaclientvideo.dll.def` and import proxy already provide `CVideoPlayerUtility2` exports 135–159, including `NewL`, both `AddDisplayWindowL` overloads, extent/clip/rotation/scale, `AddDisplayL`, `RemoveDisplay` and external-display control. No ordinal or signature was invented. `surfaceeventhandler.h` intentionally forward-declares `TSurfaceId`; consumers that use its fields include `<graphics/surface.h>` too. Subtitles retain the official `SYMBIAN_MULTIMEDIA_SUBTITLE_SUPPORT` guard.

The SDK already includes `RegisterAudioResourceNotification`, `CancelRegisterAudioResourceNotification` and `WillResumePlay` declarations and exports for `CMdaAudioPlayerUtility` (31, 34, 20), `CMdaAudioOutputStream` (15, 16, 14) and `CVideoPlayerUtility` (108, 109, 107). These APIs need no replacement declarations. Availability and behavior on a device or emulator still require runtime verification.
