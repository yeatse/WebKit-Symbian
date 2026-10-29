# Browser media integration evidence

Outcomes are tied to each recorded browser and emulator hash. A passing page
report verifies API behavior; actual pixels and PCM are checked separately.

| Evidence | API | Presentation |
| --- | --- | --- |
| [First media attempt](first-failure/README.md) | FAIL: pending play Promise | No playback claim |
| [d844 partial HTTP](partial-http-d844/README.md) | FAIL: access violation after 11 groups | Video visible; 30.75 ms interior audio dropout |
| [LLInt diagnostic](historical-llint8955/README.md) | FAIL: 13/14 with old ended-event fixture | Eight named PCM windows pass; no JIT acceptance |
| [First inline-cache candidate](historical-icfix86877/README.md) | FAIL: full HTTP still crashes | Limited ten-iteration progressive diagnostic passes separately |
| [2db5 / ff218 HTTP](candidate-http2db5/README.md) | PASS: 14/14 | Real video motion/pause; eight PCM windows and MP3 tone pass |
| [2db5 / ff218 TLS](candidate-tls2db5/README.md) | PASS: 17/17; temporary CA restored | FAIL: 33.9167 ms interior video-audio dropout; seven of eight PCM windows pass |
| [2db5 / 12c275 HTTP](candidate-http2db5-12c275/README.md) | PASS: 14/14 | Real video motion/pause; eight PCM windows and MP3 tone pass |
| [2db5 / 12c275 TLS](candidate-tls2db5-12c275/README.md) | PASS: 17/17; temporary CA restored | Real video motion/pause; eight PCM windows and MP3 tone pass, independently reproduced twice |

The [natural-end test correction](natural-end-fixture.md) waits for a queued
event instead of prematurely cleaning up when the property changes. The
[window alpha review](window-alpha-review.md) explains the official native
window contract and software page fallback. Neither correction rewrites old
failed results.

The [progressive-reader host probes](progressive-read-ahead/README.md) reproduce
the cached short-packet mechanism and verify patch 0017's bounded audio
prefetch. They preserve permanent-truncation failures. The new `12c275...`
emulator passes the [native Utility2 rerun](../media-native/video-prefetch12c275/README.md)
and both complete browser media suites above. The ff218 TLS recording remains
a historical failure.

The candidate directories contain entire captures as lossless FLAC, original
screenshots, reports, hashes and independent audio analyses. No recording was
cropped, normalized or repaired. With FFmpeg and NumPy 2.3.5 installed, run:

```sh
python3 verify-captures.py
```

The verifier decodes the full PCM, checks its original byte count and SHA,
then reruns the exact control-window and MP3 analyses in a temporary directory.
Successful verification means the archived observations were reproduced,
including the recorded TLS failure. The optional directory arguments select
other compatible captures; they do not change acceptance thresholds.
