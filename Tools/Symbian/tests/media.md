# Audio and video playback

The browser suite checks media metadata, playback progress, pause, seek, volume
and completion over HTTP and HTTPS. Requires FFmpeg, ffprobe and OpenSSL.
Configure EKA2L1 using [Testing](../docs/TESTING.md).

```sh
python3 Tools/Symbian/tests/make-media-fixtures.py /tmp/media-fixtures
bash Tools/Symbian/tests/make-test-certificates.sh /tmp/media-tls
python3 Tools/Symbian/tests/serve-media-tests.py --fixtures /tmp/media-fixtures \
  --reports /tmp/media-reports --tls-dir /tmp/media-tls
```

Run the browser suite in another terminal:

```sh
python3 Tools/Symbian/tests/run-browser-media-tests.py /path/to/browser.exe \
  --fixtures /tmp/media-fixtures --reports /tmp/media-reports \
  --ca /tmp/media-tls/ca.crt --timeout 180
```

The runner collects page reports and browser logs. Passing playback advances,
pauses, seeks and reaches completion; inspect the fixture video and audio during
playback to check presentation.
