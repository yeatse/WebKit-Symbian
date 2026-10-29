# Media host evidence

This directory records host-side checks, not successful Symbian playback.

- `download-tests.log`: the actual EKA helper passes 5 groups containing 13
  requests, including success, redirect restrictions, cancellation and file
  removal, HTTP errors, and three distinct TLS certificate cases.
- `media-tls-preflight.json`: listener identity and verified certificate
  rejection reasons for ports 8776–8778; disposable host contexts only.
- `fixtures.json`: generated 12-second audio/video hashes and decoder metadata.
- `chromium-sanity.json`: Chromium passes 13 of 14 page groups. It waits after
  the deliberately truncated response instead of reporting an immediate
  media error. The Symbian curl path must still report the transfer failure;
  this case is not relaxed for the host browser's different behavior.
- `result.json`: source hashes and limits of these observations. The truncated
  route was separately confirmed to deliver 236921 of 710764 advertised bytes.

The media runner reuses strict process cleanup, fresh reports/console checks
and temporary CA restoration. Its supporting network runner's 12 host
regressions pass after the configurable-origin addition. Running the page
does not establish decoded video or audible sound; capture and inspect actual
frames and private Pulse PCM before declaring media acceptance complete.

Reproduction commands and native MMF checks are documented in
`../../../tests/media.md` and `../../../tests/native-mmf/README.md`. Host
dependencies require libcurl 7.85 or later with working TLS verification.
No test private keys, emulator data, binary SDK, or generated media are saved
here.
