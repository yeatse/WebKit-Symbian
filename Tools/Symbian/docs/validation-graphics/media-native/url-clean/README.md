# Native URL errors and cancellation on the clean host

All runs use the recorded clean host, native-only DLLs and heap-fixed probe;
each has fresh logs, guest/host exit zero and no panic. HTTP 404 reports
`KErrNotFound` (-1). Wrong hostname and untrusted TLS root each report
`KErrAccessDenied` (-21). The preflight confirms the three live certificate
identities and specific hostname/root verification failures, retaining strict
verification. Only the native emulator process uses the public test CA.

The representative `cancel/` run uses probe `5fb5a4af…`: Close occurs at
265 ms and the observation ends at 3328 ms, covering 3063 ms without any old
open callback. This exceeds the server's two-second delayed response. Guest
and host exit zero, with no panic and zero open callbacks.

The initial 1.5-second observation is preserved under `cancel-1500ms/` with
its original `afcaa039…` probe hash. That shorter window did not cover the
server delay. Other native PCM and URL runs retain their actual old probe
identities; none is relabelled as having run the stronger cancellation probe.

These tests exercise native OpenUrlL. Browser HTTP(S) media uses a separate
WebCore loader and needs its own integration results.
