# Native Utility2 after the prefetch correction

The `12c275e7...` emulator, `5fb5a4af...` probe and recorded native-only MMF
DLLs pass all 16 API/surface checks, with guest and host exit 0 and no panic.
The unmodified screenshots show frame 80 at 5.333 seconds, then frame 111 at
7.400 seconds. After Close, the black video area and white border remain with
no stale frame.

The complete 48 kHz stereo PCM capture is preserved as lossless FLAC. Decoding
it reproduces raw SHA `279b99bf7bbfd7a016813a0a4af802712dcdc5cda6a08ae67ec37c2a2793c8c8`.
The existing native audio analyzer independently reproduces all recorded
checks: 880 Hz tone, pause/resume/stop silence and timing, no clipping, and no
unexpected exact-zero interval of at least 1 ms. No thresholds were changed.

The original runner and host-log hashes remain in `evidence.json`; the runner's
audio filename and analysis command path are made portable. For reanalysis,
decode `audio.flac` to `audio.raw` in a temporary directory, copy `result.json`
and `mmf-probe.log` there, and run `../audio/analyze-native-audio.py` against that
directory with `--frequency 880`. Original captures with earlier host or probe
hashes remain separate.
