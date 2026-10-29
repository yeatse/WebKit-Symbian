# TLS media candidate 2db5: API PASS, audio FAIL

All 17 page groups pass on the exact `2db5f849...` browser and clean
`ff218e2e...` emulator. The strict runner completes its 180-second observation
and normal window shutdown with no guest access violation or panic. Trusted
HTTPS playback and rejection of wrong-host and untrusted-root certificates
pass. Host preflight proves all three listeners are reachable with the intended
certificates, then verifies rejection codes 62 and 18. The temporary guest CA
bundle is restored exactly.

Overall media presentation remains **FAIL**. Of the eight named PCM control
windows, seven pass the unchanged criteria. Initial video playback contains a
33.9167 ms exact-zero gap at PCM time 21.745021–21.778938 seconds, followed by
another 456.271 ms of 880 Hz audio before the sustained pause. This is an
interior dropout, not a pause-boundary timing ambiguity. The failing video
control case uses ordinary HTTP even in this TLS suite; the evidence does not
attribute the dropout to TLS. The separate MP3 observation passes three
660 Hz windows.

The entire capture is retained in lossless `audio.flac`, including the failure
and idle remainder. `audio-flac-verification.json` proves its decoded bytes
match the original raw capture SHA. No clipping, cropping, normalization,
resampling, or replacement silence was applied. The analysis covers named
control windows, not every transition in the complete recording.

Seven unmodified screenshots independently show real decoded video. In the
5,568-pixel rectangle `(274,137,390,185)`, playing frames differ in 1,410 RGB
pixels, paused frames differ in zero, and resumed frames differ in 2,058. MMF
video uses the documented software page fallback. The operator's separate
pixel report uses its own recorded rectangle and channel-count units.

The host log shows partial-file/NAL errors and reopening of the first control
video, as in the earlier d844 failure. The successful HTTP run has only one
open of that video. This supports a progressive-decoder investigation; without
synchronized decoder traces it does not establish the exact cause of the gap.

API, pixel and PCM outcomes are separate fields in `evidence.json`. The API
runner's original `PASS` is retained as an API result; it is not promoted into
overall media-quality success.
