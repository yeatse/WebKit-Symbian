# Audio startup investigation and independent state fixes

The first-play failures were reproduced with one emulator, probe and native DLL
set. `startup-comparison.json` records their identical SHA-256 identities,
measurements, and original log hashes. The adjacent logs are filtered excerpts;
full logs and PCM captures remain in the recorded validation runs.

| Capture setup | Synchronous Play duration | Position at first 1.5 s check | API result |
| --- | ---: | ---: | --- |
| capture-50ms-and-pcm-ready | 730 µs | 1493287 µs | PASS |
| no-capture | 639 µs | 0 µs | FAIL |
| old-default-capture-without-readiness | 1078 µs | 225668 µs | FAIL |

The failed runs supplied complete, nonzero decoded frames and had no false pause
state or silent-frame deduction. Play returned in less than 1.1 ms, ruling out
the proposed decoder starvation and synchronous stream-initialization delay for
these examples. No decoder or implicit-seek workaround was applied, and neither
the probe's clock origin nor its acceptance thresholds were relaxed.

The private PulseAudio 17 null sink defaults to a two-second idle render block.
Its requested-latency handler changes the next block size without rolling back
the already-rendered timestamp. `pulse-latency-audit.json` identifies the official
source, hash, relevant lines and sampled sink configuration. Starting a recorder
with a 50 ms latency request and waiting for at least 19,200 bytes of actual PCM
before launching the guest established a running measurement chain. That run
passed the API checks and the separately archived strict 660 Hz PCM analysis.
The initial readiness wait was 1.917 s, consistent with the idle-render remainder.
The server configuration was not changed.

This is an identified limitation of the private headless validation sink. The
no-capture run also failed while it was cold; waiting for recorder readiness
establishes a defined validation starting state, not a change to the audio
engine. These records do not prove startup behavior on every host audio device,
and prior cold-sink failures remain recorded. PCM analysis and final native-media
acceptance are archived separately under `media-native`.

## Independent production corrections

`0015-Make-audio-pause-state-and-position-accounting-thread-safe.patch` fixes two
separate source defects found while investigating:

- The render callback read the pause flag and incremented idle-frame accounting
  while the control thread accessed the same non-atomic fields. Both are now
  atomic. Position uses a single counter snapshot for comparison and subtraction,
  and the count is cleared before callbacks can start.
- Failure to create an output stream previously returned successful Play. It now
  marks playback complete and returns failure to the MMF dispatcher.

These corrections are not credited with fixing the null-sink startup delay.
Temporary callback/timing logs were removed from production. Both changed C++
translation units passed syntax checks with the actual EKA host compiler flags.
The final emulator/guest regression is a separate validation step.

`patch-chain.json` records successful sequential apply checks of patches 0001,
0005–0015 against EKA base `5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a`. The check
used a temporary Git index and did not modify the source checkout's real index.
