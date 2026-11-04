# Native audio and video

Uses Symbian^3 `CVideoPlayerUtility2` and `CMdaAudioPlayerUtility` for native
playback. Build with the Symbian SDK and converter:

```sh
export SYMBIAN_SDK_PREFIX=/path/to/sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native
export PROBE_BUILD=/tmp/native-mmf
Tools/Symbian/tests/native-mmf/build.sh
```

Deploy `e32/mmf_probe.exe` and the [media fixtures](../media.md), then launch:

```text
mmf_probe.exe video file C:/Data/WebKit/media/video.mp4 10000
```

Use a clip at least 12 seconds long. The probe opens and prepares playback,
checks position, pause, seek and resume, and displays video in a native window.
`C:/Data/WebKit/mmf-probe.log` contains its result. Passing checks exit with code
zero; inspect the video and audio during playback to check presentation.
