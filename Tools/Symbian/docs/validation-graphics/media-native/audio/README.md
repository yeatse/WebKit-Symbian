# Native MMF PCM evidence

Three complete clean-host captures are stored as lossless FLAC, totaling 433,859 bytes.
Decoding each produces the exact original interleaved s16le / 48 kHz / stereo
PCM: byte length and SHA-256 are recorded in `manifest.json`. No samples were
trimmed, normalized, resampled, or otherwise changed. The MP3 and MP4 fixture
decoders had already produced this PCM; FLAC only compresses the monitor capture.

| Capture | Independent PCM result | Scope |
| --- | --- | --- |
| `wav-local-clean` | PASS: 440 Hz initial/resumed playback, pause/stop silence and continuity | Clean host `ff218e2e…`, final native-only DLLs; full FLAC retained |
| `mp3-local-clean` | PASS: 660 Hz initial/resumed playback, pause/stop silence and continuity | Same clean host/DLLs; full FLAC retained |
| `video-local-clean` | PASS: 880 Hz initial/resumed playback, pause/stop silence and continuity | Same clean host/DLLs; full FLAC retained; pixels maintained separately |
| `wav-https-clean` | PASS: 440 Hz through verified HTTPS, controls and continuity | Same clean host/DLLs; original raw remains external |
| `wav-short-tail-clean` | FAIL: only 98.02 ms post-stop capture, below the unchanged 100 ms requirement | Tone, controls and continuity passed; preserved insufficient-coverage result |
| `mp3-short-tail-clean` | FAIL: only 99.625 ms post-stop capture | Same fixed coverage requirement; preserved failure |
| `mp3-native-only-candidate` | PASS: 660 Hz initial and resumed intervals, pause and stop silence, no unexpected gap over 1 ms | New native-only DLLs and heap-fixed probe; host `6b31d8e5…` still contains temporary diagnostics |
| `video-native-only-candidate` | PASS: 880 Hz initial and resumed intervals; pause/control offset spread 6.02 ms; no unexpected gap over 1 ms | Same candidate host and DLLs; surface/pixel evidence is maintained separately |
| `wav-http-historical` | PASS: 440 Hz through HTTP, including pause/resume/stop and continuity | Earlier host/probe and DLL deployment; this does not validate the final native-only DLLs |
| `wav-gap-historical` | FAIL despite API 8/8: a 6.25 ms unexpected silent gap | Preserved historical failure, external raw PCM |
| `mp3-silent-start-historical` | FAIL: API 6/8 and no initial audible interval; resumed 660 Hz was present | Preserved historical failure, external raw PCM |
| `video-short-capture-historical` | FAIL: first capture segment cannot supply two complete FFT windows; resumed 880 Hz passes | Preserved insufficient-coverage result, external raw PCM |

Each directory retains the original guest log, runner result and analysis JSON.
The clean successful runs and earlier native-only candidates also retain
compressed original host logs. The manifest records source-run locations and hashes. Those runs'
`result.json` includes exact host, probe, DLL and map hashes. Earlier results did
not record deployed DLL hashes and must not be retroactively attributed to the
new package. The four clean successes are distinct from the preserved trace-host candidates.

## Reproduce without EKA

Install Python 3, NumPy and FFmpeg, then run from any working directory:

```sh
python3 Tools/Symbian/docs/validation-graphics/media-native/audio/verify-captures.py
```

This verifies all archived metadata hashes, decodes the three FLAC files into
temporary directories, checks each decoded PCM against the original raw SHA,
and reruns the exact archived analysis script. It compares the result, every
check, PCM measurements, audible intervals and silent spans with the recorded
JSON. Runs without committed audio receive metadata verification
only; they are not silently counted as fresh PCM tests. No guest, XDG state or
repository evidence is modified.

For an external original capture, `analyze-native-audio.py RUN --frequency HZ`
expects `audio.raw`, `mmf-probe.log` and `result.json`; it writes the analysis JSON
there. Its exit code indicates report generation, while the JSON `status` and
`checks` determine the audio result.

## Sampling and limits

The unchanged analyzer uses 10 ms RMS activity windows, per-channel 250 ms Hann
FFTs (4 Hz bins), at least two interior FFT windows in each playback interval,
at least 95% energy within the expected frequency ±8 Hz, and a dominant bin
within ±2 Hz. It checks pause duration/control alignment within 100 ms, at least
100 ms of silence after stop, clipping, and unexpected exact-zero gaps over 1 ms.
These are tests of known synthetic fixtures, not a general audio-quality score.

New native-only captures request `parec --latency-msec=50` and require at least
19,200 complete PCM bytes before launching EKA, bounded by three seconds. Their
runner results preserve recorder start, first PCM readiness, emulator launch
and capture end monotonic timestamps. Final local WAV/MP3 runs also require
200 ms of actual new PCM after normal host exit, bounded by one second and the
overall deadline; tail byte counts/times are retained. No zeros are added, and
the original audio thresholds remain unchanged. Guest phase times and PCM transitions are
also retained. Individual PCM sample epochs and the guest clock are not jointly
timestamped, so the analyzer's fitted offsets are not independent measurements
of end-to-end playback latency.

`pulse-latency-audit.json` records the upstream PulseAudio 17 source and its hash.
The null sink defaults to a two-second idle render block; changing requested
latency does not itself reset an already-rendered timestamp. This explains a
plausible cold-observation delay and motivated waiting for actual PCM. It does
not erase earlier failures or prove that every initial playback problem shares
that cause. The server was not reconfigured to obtain these results.
