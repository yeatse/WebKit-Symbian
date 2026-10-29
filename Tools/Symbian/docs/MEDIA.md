# Symbian media integration

The port enables HTML audio/video through MMF. Video uses Symbian^3
`CVideoPlayerUtility2`, an actual `RWindow`, display extent and clipping;
audio uses `CMdaAudioPlayerUtility`. SDK 0.3.0's missing Utility2/surface
headers are restored from the SDK's pinned official Symbian source. Existing
proxy ordinals are used; the browser does not substitute an older video
utility merely because a public header was absent.

## Browser loading and presentation

`MediaPlayerPrivateSymbian` implements the WebKit 2.52 refcounted media-engine
interface, `MediaTime`/seek operations, loader callbacks and cancellation.
The embedding assigns a real `PageIdentifier`, which WebKit 2.52 requires
to obtain the media-session manager and complete HTML `play()` requests.
The native wrapper reports actual open, duration, audio/video dimensions,
playback and control errors. Generation checks reject callbacks from a
cancelled or replaced load. A format candidate returns `MayBeSupported`;
only opening and playing it can establish support.

Browser HTTP(S) remains in WebCore's curl loader, retaining request policy,
redirect handling, cookies, cancellation and certificate verification. Bytes
are flushed to a shared native file. Playback can begin once sufficient data
and an MP4 index are available. A trailing MP4 index requires the complete
file. Partial-file EOF enters a recoverable stall; after more data arrives,
the native player reopens and resumes at the saved position. The initial
buffer and retry thresholds are 64 KiB, with approximately five seconds of
additional data before a stalled stream resumes. Non-indexed partial-file
duration and buffered ranges use byte-based estimates, not arbitrary remote
random-access guarantees.

Local files open through MMF directly. Native video paints through its window
surface underneath a transparent area in the software-rendered page. Opening
native video makes the embedding view leave EGL page composition; this
fallback remains active for that view until its native window is explicitly
reconfigured. The implementation does not presume that MMF can show through
an opaque EGL window. Clipping, visibility, cancellation and source changes
update or remove the native display registration.

Temporary browser downloads live in `C:/Data/WebKit/MediaCache/` with the
`webkit-media-` prefix. Startup removes only those owned cache files. Local
source media and test fixtures belong in a separate directory, for example
`C:/Data/media-tests/`; a generic `Media` directory is never a cleanup target.

CPU-readable video frames are not implemented. `drawImage(video)` and video
uploads into WebGL therefore do not provide decoded frames. Canvas painting
also cannot reposition the native video window. Native presentation must be
verified from actual window pixels, not from an assumed canvas readback.
MSE, MediaStream, DRM, external-display switching and audio-resource
arbitration are not claimed. The resource-notification APIs report
`KErrNotSupported` while no corresponding host arbitration exists.

## Native OpenUrlL and EKA changes

Native `OpenUrlL` has a separate implementation for applications using MMF
directly. A detached host worker owns only shared transfer state and uses
libcurl 7.85 or later. It downloads HTTP(S) to a private temporary file before
opening the existing local FFmpeg decoder. Native OpenUrl is not progressive;
the browser's WebCore path is the progressive implementation described above.

The worker checks every curl option, restricts request and redirect protocols
to HTTP(S), limits redirects, verifies both certificate chains and hostnames,
and enforces connection/transfer deadlines. The write callback stays in the
emulator module's C runtime and detects size overflow and short writes.
Close/replacement cancels the transfer; obsolete observers are not called.
The decoder closes before its temporary-file owner is released. HTTP and TLS
failures reach the native callback with errors instead of successful opens.
An explicit unsupported IAP or controller selection is rejected.

Utility2 surface notifications use the actual exported surface identity.
Crop changes notify registered clients, and Close/RemoveDisplay notify them
before they can keep using a removed surface. The default crop is the full
decoded frame. Callback guards handle observers that reenter Close, remove a
registration or delete the utility.

Apply EKA patch `0010` after patches `0001`, `0005`–`0009`. Rebuilding only the
host is insufficient: its default build copies old prebuilt guest patch DLLs.
Use [native-media-patches](../eka2l1/native-media-patches/README.md) to rebuild
and deploy both media DLLs with the official SDK headers, preserved ordinal
tables and real native-leave unwind data. Deploy into the selected EKA resource
`patch/` directory, retaining its map files, and check the deployed hashes
after every host build. The native DLLs and locally defined native classes
must pair User-heap allocation and deallocation; the browser media adapters
use ordinary C++ ownership instead. Do not overwrite firmware ROM libraries.
EKA source changes remain local and are reproduced through exported patches.

The codec candidates currently exposed to HTML include PCM WAV, MP3,
AMR/3GP, AAC/MP4 and H.264 Baseline/MPEG-4 video. Actual support still depends
on the installed MMF controller or EKA decoder; the acceptance fixtures use
PCM WAV, MP3 and H.264 Constrained Baseline with AAC. No WebM/Opus or adaptive
streaming support is inferred from a filename or MIME string.

## Verification status

The checks are deliberately separated:

| Layer | Current evidence |
| --- | --- |
| SDK/native DLL build | Native-only audio/video ELF and E32 builds pass preserved-export, native-leave and User-heap checks; neither imports libc/pthread or itself. |
| Host URL helper | 13 requests in 5 groups pass, including verified TLS success, hostname/root rejection, cancellation, protocol restrictions and cleanup. |
| Native WAV | Clean-host native-only DLL playback passes 8/8 with 440 Hz PCM, pause/resume/stop silence and continuity. HTTPS WAV also passes the independent analysis. Earlier HTTP evidence remains separately identified. |
| Native MP3 | Clean-host native-only DLL playback passes 8/8 and independent 660 Hz PCM analysis with monitor PCM ready before launch. Cold-Pulse failures are preserved. |
| Native Utility2 video | Clean host passes API/surface 16/16 and 880 Hz AAC PCM analysis. Decoded frames change from 5.267 s / frame 79 to 7.400 s / frame 111; Close leaves no stale frame. |
| Browser page | The Page-ID fix advances the JIT HTTP candidate through 11 passing groups and shows real video through the software fallback, but an access violation after progressive cleanup and a 30.75 ms interior PCM dropout leave the run failed. A separate LLInt diagnostic reaches all 14 groups without that crash; its 13/14 result exposed an ended-event test race. The fixture now waits for the queued event with the same deadline and strict count. Fresh JIT HTTP/TLS runs are still required. |

The [native-only DLL build evidence](validation-graphics/media-native/native-build/README.md)
and [lossless PCM captures](validation-graphics/media-native/audio/README.md)
record exact identities and reproducible analyses. Historical WAV evidence
uses the earlier DLL package and is not attributed to the newer package.

The [first HTML integration failure](validation-graphics/media-browser/first-failure/README.md)
is retained with its original identities and failure status.

These observations do not constitute a complete media acceptance result.
The [host evidence](validation-graphics/media-host/README.md) includes the
fixture hashes and a Chromium sanity run. Chromium passes 13/14 groups but
waits on the deliberately truncated response; the Symbian curl path must
still propagate that partial-transfer failure. This requirement is not waived.

The [clean-host Utility2 evidence](validation-graphics/media-native/video-clean/evidence.json)
records the real surface callbacks, changing pixels and removal. The
[native URL checks](validation-graphics/media-native/url-clean/README.md)
record precise HTTP/TLS errors and a successful three-second post-Close
cancellation observation, covering the delayed response.
Browser integration remains a separate outstanding check. The [audio startup investigation](validation-graphics/audio-startup/README.md)
retains the same-image cold-sink failures and PCM-ready success. The private
Pulse null sink renders a two-second idle block, so validation waits for actual
monitor PCM before launching the guest. This establishes the measurement
starting state; it does not establish startup latency on every host audio device.
Patch `0015` independently fixes shared audio-state races and output-stream
creation failure reporting; it is not credited with fixing that sink delay.

Use [media.md](../tests/media.md) for fixture generation, listeners, strict
browser automation and temporary test-CA restoration. Use the
[native MMF probe](../tests/native-mmf/README.md) for native URL cancellation,
pause/seek, errors and Utility2 surface events. Retain the exact browser,
probe, host, DLL and fixture hashes, fresh logs and reports. Final acceptance
also needs changing video frames and captured PCM with the expected tones,
pause/mute silence and resumed playback. Test CA keys and SDK/runtime binaries
are not repository artifacts.
