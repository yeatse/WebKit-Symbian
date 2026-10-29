# TLS media with corrected prefetch: PASS

The unchanged `2db5f849...` browser and corrected `12c275e7...` emulator pass
all 17 groups and complete 180 seconds with normal host shutdown, no panic or
access violation. Trusted HTTPS plays; hostname and untrusted-root cases fail
as expected. Preflight proves the intended certificates and live endpoints,
and the temporary CA bundle is restored exactly.

All eight named PCM control windows pass the original FFT, RMS, silence and
gap checks. The complete initial video interval, 20.1–22.085708 seconds in
the recording, has no interior exact-zero gap of at least 1 ms. The independent
MP3 check passes three 660 Hz windows. Two independent analysis runs produce
identical JSON and lossless FLAC; the 34,837,024 decoded PCM bytes match raw SHA
`b527ec489b395aa6420322389c28a7bfbaf57470753b3682f5dffe77bf09b5f8`.

Original screenshots show decoded video movement, pause and resumed movement.
In the 5,568-pixel rectangle `(274,137,390,185)`, the three pairs differ in
1,242, zero and 2,058 RGB pixels respectively. The MMF native background uses
the documented software page fallback. No GPU/MMF simultaneous composition is
claimed.

These results validate this exact candidate and the named fixtures/controls.
They do not assert uninterrupted audio during the deliberately stalled network
case or source changes. The full recording, reports, hashes and earlier
`ff218...` failure remain intact, with each run's identity kept separate.
