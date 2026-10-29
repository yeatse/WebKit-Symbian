# First inline-cache candidate: historical FAIL

The `86877e0a...` candidate passes the limited progressive-cleanup diagnostic
for ten iterations. Its subsequent full HTTP run still crashes after eleven
passing console groups, before the remaining groups and final page report.
These are different runs and both outcomes are preserved.

The full run reads `0xE1A00000` with PC `0x1bb3b600`, LR `0x1ba139f8`, and
terminates with `KERN-EXEC 3`. The console, strict runner result, crash excerpt
and artifact hashes remain **FAIL**. It uses the corrected ended-event fixture
with SHA `e46cf9432c9a26d8394ee59dab6b69590664d1b87135f33bfcbc29de87a070e8`.

Capture/screenshot identities are retained as external artifacts. No PCM
semantic success or complete presentation success is claimed from their
presence. The later inline-cache boundary candidate requires independent
HTTP/TLS and presentation runs; these results are not relabeled for it.
