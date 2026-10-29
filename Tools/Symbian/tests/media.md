# Media acceptance

`media.html` verifies actual metadata, advancing playback position, pause,
seek, mute/volume controls and end/error events. It contains 6 local groups,
14 HTTP groups, or 17 HTTP/TLS groups with `tls=1`. Format rejection is one
group; a `canPlayType` result is never accepted as playback evidence.

Generate fixtures outside the repository:

```sh
python3 Tools/Symbian/tests/make-media-fixtures.py /tmp/media-fixtures
bash Tools/Symbian/tests/make-test-certificates.sh /tmp/media-tls
python3 Tools/Symbian/tests/serve-media-tests.py --fixtures /tmp/media-fixtures \
    --reports /tmp/media-reports --tls-dir /tmp/media-tls
```

FFmpeg with libx264, AAC and libmp3lame encoders is required. Fixtures last
12 seconds: PCM WAV at 440 Hz, MP3 at 660 Hz, and H.264 Constrained Baseline
320×180/15 fps with AAC at 880 Hz. The manifest records bytes, SHA-256,
duration and decoder metadata. The MP4 pair has either a leading or trailing
index. The deliberately invalid file must produce a real media error.

Open `http://localhost:8775/media.html?autorun=1&run=UNIQUE_TOKEN&tls=1`.
Use a fresh token for each launch; the server rejects duplicate reports.
Keep the report, exact image/host/DLL/fixture hashes and this launch's console
and emulator logs. Allow at least 120 seconds for all groups. Any missing
group, timeout, panic, nonzero guest/host exit, or absent fresh report fails
the run. For local playback, copy the page and fixtures into the same guest
directory and use `file:///C:/Data/WebKit/media/media.html?autorun=1&run=TOKEN`.
The local report is the unique console `RESULT` JSON.

For HTTP/TLS page automation, use the shared strict browser runner:

```sh
python3 Tools/Symbian/tests/run-browser-media-tests.py /path/to/browser.exe \
    --fixtures /tmp/media-fixtures --reports /tmp/media-reports \
    --ca /tmp/media-tls/ca.crt --timeout 180
```

It inherits the emulator lock, normal window shutdown, bounded descendant
cleanup, fresh unique report/console agreement and exact CA restoration
from the network runner. Its manifest leaves `presentation_verified` false:
recording and checking actual audio/video remains a separate acceptance step.

HTTP routes support regular, `slow-`, `delayed-`, `stall-`, and `truncated-`
prefixes on fixture names. Delayed sends no response for 2 seconds. Slow
sends 16 KiB per 180 ms. Stall stops for 6 seconds after the first 128 KiB;
the page requires a recoverable partial-file stall and subsequent position
advance, without a premature `ended` event. `/transfer-state?token=...`
records transmitted bytes and completion, so progressive playback must
start before the response finishes. The trailing-index MP4 must wait until
the whole file is available. HTTP cancellation/replacement uses an open
that is still pending and checks for late metadata/error callbacks.

TLS listeners are 8776 (trusted localhost), 8777 (wrong hostname), and
8778 (untrusted root). Before negative guest cases, call the existing
`run-browser-network-tests.py` module's `check_tls_fixtures` with ports
`{"good":8776,"wrong":8777,"untrusted":8778}` and retain its identity and
specific verification-error evidence. This distinguishes certificate failure
from a dead listener. Temporarily add only the public test CA to the isolated
emulator CA bundle using that runner's `temporary_test_ca`, restoring exact
original bytes or the original absent-file state in `finally`. Never install
the test CA into SDK, package or global trust. Do not archive private keys.

Native `OpenUrlL` uses the separate host helper. For its TLS tests only, set
`EKA2L1_MEDIA_CA_BUNDLE` to the public test CA in the emulator process's
environment. This does not change guest/browser trust. Native OpenUrl waits
for the complete download; the browser uses its WebCore loader and the
progressive shared-file path. See `native-mmf/README.md` for Utility2 surface
events and native API tests.

Position and event success does not prove presentation or audible output.
During `CAPTURE` intervals record the private Pulse monitor and compare the
expected tone with pause/mute silence and resumed tone. Capture at least two
distinct video frames during playback and verify the test pattern changes;
also retain a screenshot after hiding/removing the video to catch stale
native surfaces. The page intentionally makes no claim based on a canvas
copy of a native video surface. In this phase, opening native video makes
the view fall back to software page composition; EGL/MMF alpha composition
is not presumed to work.
