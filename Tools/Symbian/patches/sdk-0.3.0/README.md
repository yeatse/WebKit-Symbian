# Missing Symbian^3 SDK headers

The 0.3.0 SDK's Video inventory selected `videoplayer.h` and `videorecorder.h`, omitting the graphics-surface extension headers. These files are unmodified upstream headers, including their original copyright and Eclipse Public License notices, from the exact revisions already recorded in the SDK's `share/symbian/native/sources.json`:

- [videoplayer2.h](https://github.com/SymbianSource/oss.FCL.sf.os.mmaudio/blob/a39c075cce88671b23d13598fda2f7cbf008f3bb/mmlibs/mmfw/inc/videoplayer2.h) and [surfaceeventhandler.h](https://github.com/SymbianSource/oss.FCL.sf.os.mmaudio/blob/a39c075cce88671b23d13598fda2f7cbf008f3bb/mmlibs/mmfw/inc/surfaceeventhandler.h), from Symbian OS Multimedia Audio.
- [graphics/surface.h](https://github.com/SymbianSource/oss.FCL.sf.os.graphics/blob/ff133bc50e6158bfb08cc093b0f0055321dcde99/graphicsutils/commongraphicsheaders/inc/surface.h), from Symbian OS Graphics.

`manifest.json` records the source repository, revision, path, Git blob SHA-1 and file SHA-256 for each header. `scripts/apply-sdk-patches.py` validates the SDK version, source revisions, hashes and all existing destinations before adding missing files. Reapplication is harmless; it refuses to overwrite a different file. It performs no network access and changes no existing SDK ABI, library or proxy.

The existing `mediaclientvideo.dll.def` and import proxy already provide `CVideoPlayerUtility2` exports 135–159, including `NewL`, both `AddDisplayWindowL` overloads, extent/clip/rotation/scale, `AddDisplayL`, `RemoveDisplay` and external-display control. No ordinal or signature was invented. `surfaceeventhandler.h` intentionally forward-declares `TSurfaceId`; consumers that use its fields include `<graphics/surface.h>` too. Subtitles retain the official `SYMBIAN_MULTIMEDIA_SUBTITLE_SUPPORT` guard.

The SDK already includes `RegisterAudioResourceNotification`, `CancelRegisterAudioResourceNotification` and `WillResumePlay` declarations and exports for `CMdaAudioPlayerUtility` (31, 34, 20), `CMdaAudioOutputStream` (15, 16, 14) and `CVideoPlayerUtility` (108, 109, 107). These APIs need no replacement declarations. Availability and behavior on a device or emulator still require runtime verification.
