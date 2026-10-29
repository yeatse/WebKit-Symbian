# Native-only Utility2 candidate pixels

This run uses the native-only MMF DLLs and the probe with matching native-heap
deallocation. `result.json` records their exact hashes. The emulator still
contains temporary audio diagnostics; this is not the final clean-host run.

The unmodified window captures show the decoded test pattern at 5.200 seconds
(frame 78), then 7.333 seconds (frame 110), with visibly changed shapes.
`screenshot-final.png` shows the same probe border after Close, with a black
video area and no stale decoded frame. The probe also passes all 16 API/surface
checks and exits with guest and host status zero.

The independent [880 Hz AAC PCM evidence](../audio/README.md) belongs to this
same run. The lossless audio is archived there, not duplicated here.
`evidence.json` hashes the portable log, result and original screenshots.

These captures supersede the earlier candidate's DLL identities for this
specific run; they do not change the recorded outcome of earlier tests.
