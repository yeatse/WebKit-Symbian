# HTTP media with corrected prefetch: PASS

The unchanged `2db5f849...` browser and corrected `12c275e7...` emulator pass
all 14 HTTP groups. The fresh page report agrees with the console and strict
runner, which completes 180 seconds with normal shutdown and no guest panic
or access violation. The real stalled-transfer recovery, partial-download
playback and truncated-response error checks all remain enabled and pass.

Actual decoded video changes while playing and stays fixed while paused.
Original screenshots are retained. Comparing the 5,568 pixels inside
`(274,137,390,185)` gives 1,142 changed RGB pixels while playing, zero while
paused and 2,140 after resuming. The page switches to software painting for
native MMF video, as recorded in the console.

All eight named PCM controls pass unchanged spectral, RMS, silence and gap
criteria. The complete initial video interval has no interior exact-zero gap
of at least 1 ms. The independent MP3 observation also verifies 660 Hz. The
entire recording is losslessly preserved; its decoded SHA and both analyses
were independently reproduced using `../verify-captures.py`. These are named
window observations, not a claim of uninterrupted sound during expected
network stalls, source changes or the idle remainder.

Exact browser/host/ICU/fixture/DLL and capture identities are in
`evidence.json`. This is a new run after emulator patch 0017; the earlier
`ff218...` TLS dropout remains an archived failure.
