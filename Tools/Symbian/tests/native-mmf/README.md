# Native MMF acceptance probe

This standalone E32 executable uses the SDK's real `CVideoPlayerUtility2`,
`CMdaAudioPlayerUtility`, active scheduler and Window Server APIs. The video
path creates a visible `RWindow` and registers it using `AddDisplayWindowL`
after the open callback. It does not use WebKit or special emulator interfaces.

Build serially with `bash build.sh`. The default result is
`../native-probe-build/e32/mmf_probe.exe`. Set `PROBE_BUILD`,
`SYMBIAN_SDK_PREFIX`, `SYMBIAN_NATIVE_CONVERTER` and `WEBKIT_SOURCE_DIR` when
using other locations. The restored official Symbian^3 media headers are
required. `symbian_enable_native_leaves` supplies the SDK leave ABI.

The executable has UID3 `0xe00008f4`, NetworkServices capability, a 256 KiB
stack and a 16 MiB maximum heap. This probe does not record microphone input.

## Invocation

```text
mmf_probe.exe <audio|video> <file|url> <source> [hold-ms]
    [--timeout-ms N]
    [--cancel-open-ms N]
    [--expect-open-error <negative-code|any>]
    [--resource-notify]
    [--surface-events]
```

The probe reads `User::CommandLine` when the SDK startup supplies only its dummy
`argv[0]`. Pass the arguments after the executable name, as usual. Single or
double quotes group paths with spaces; backslashes are preserved literally.
Unterminated quotes, non-ASCII input, more than 30 arguments, and command lines
longer than 2048 characters are rejected.

Each launch tests one mode and truncates `C:\Data\WebKit\mmf-probe.log`.
The same timestamped messages go to `RDebug`. Archive the log before the next
launch. Source paths and URLs must be ASCII; file path slashes are converted
to Symbian backslashes. Examples:

```text
mmf_probe.exe audio file C:/Data/WebKit/media/audio.wav 10000
mmf_probe.exe audio file C:/Data/WebKit/media/audio.mp3 10000
mmf_probe.exe video file C:/Data/WebKit/media/video.mp4 10000
mmf_probe.exe audio url http://127.0.0.1:8775/audio.wav 10000
mmf_probe.exe video url http://127.0.0.1:8775/video.mp4 10000
mmf_probe.exe video url http://127.0.0.1:8775/slow-video.mp4 --cancel-open-ms 0
mmf_probe.exe audio url http://127.0.0.1:8775/missing.wav --expect-open-error -1
mmf_probe.exe audio file C:/Data/WebKit/media/audio.wav --resource-notify
mmf_probe.exe video file C:/Data/WebKit/media/video.mp4 --surface-events
```

The media server owner supplies actual URL endpoints. The examples do not
assert that the emulator or a device currently supports them. WAV, MP3 and
H.264 baseline/AAC MP4 fixtures should last at least 12 seconds; files shorter
than six seconds cannot exercise the full control sequence and fail explicitly.

## What is checked

- An open callback must arrive after the open API returns. Synchronous leaves
  are logged separately, including in an explicitly expected failure case.
- Video runs the separate Prepare callback. Audio preparation is its
  `MapcInitComplete` notification.
- Playback must advance its reported position by more than 250 ms after
  1.5 seconds. Pause is held for 600 ms and permits no more than 100 ms drift.
- Seek while paused targets the lesser of half the clip and three seconds;
  the returned position must be within 500 ms. Resume must advance again.
- The window remains visible and playback continues for the requested hold
  period (10 seconds by default), followed by Stop. Natural completion after
  the control sequence is recorded without treating the remainder as audio.
- `--cancel-open-ms` closes a still-pending open at the requested delay after
  calling Open, then observes 1.5 seconds without open/prepare/play callbacks.
  A server that completes before the cancellation deadline fails that case;
  use a delayed response or delay zero to exercise cancellation reliably.
- Expected-error cases pass only for the explicitly requested error. `any`
  permits any nonzero error and still records the exact code and callback path.
- Optional resource notification registration after preparation and cancellation must succeed.
  Resource-available callbacks are recorded and call `WillResumePlay`. No
  callback is assumed when the audio resource was already available.
- Optional video-only `--surface-events` also registers `AddDisplayL`, checks
  non-null surface identities, changes the crop while paused, and observes
  matching creation/change/removal callbacks through Close. It keeps the same
  visible window and waits one second after Close for removal. This is separate
  from `--resource-notify`, whose registration lifetime ends before Close.

The internal timeout defaults to 30 seconds, uses the monotonic system tick
count and stops the active scheduler on expiry. The timer cannot interrupt a
native API that blocks its thread; the launcher must also enforce a host
deadline (for example 45 seconds for the default invocation).

`RESULT` contains callback/check counts and the process exits 0 for passing
checks, 1 for failures, or 2 for malformed arguments. Every unexpected native
leave, callback error, timeout or unsupported required operation remains a
failure. The probe does not simulate a successful callback.

## Separate presentation and sound evidence

A passing position or Play callback is not proof of decoded pixels or sound.
Capture the video window during its hold interval and verify actual fixture
frames. Capture the private Pulse monitor throughout audio/video playback and
verify the fixture's known PCM frequencies and time intervals. Keep those
artifacts alongside the event log, exact E32 hash, fixture hashes and emulator
patch/build identity. No emulator run is performed by `build.sh`.

## Host command-line regression

Run `bash run-command-line-test.sh` with a C++17 host compiler (`CXX` may
override `c++`). The test includes the production `CommandLine.h` and checks
quoted paths/URLs, preserved backslashes, empty input, unmatched quotes and
argument-array overflow. It does not call `User::CommandLine`, exercise the
SDK startup, or execute MMF; those require the E32 probe on a guest/device.

Cancellation observes callbacks for three seconds after Close, exceeding the
media fixture server's two-second delayed response. The run fails if any old
open callback arrives during that interval. Playback timing thresholds are
unchanged.
