# Browser video window alpha contract

The pinned official Symbian^3 NGA implementation is identified by URL, revision,
function lines and original byte hash in `window-alpha-review.json`.
`SetRequiredDisplayMode` is deprecated there and returns the system display
mode without enabling transparency. The alpha API separately enables window
transparency and reports errors. Its documentation also states that invalid
areas are cleared to the window background before drawing.

The browser already paints video holes as transparent black into the Cairo
ARGB bitmap and uses `EDrawModeWriteAlpha` for its bitmap copy. ConstructL now
also enables the native window alpha channel and sets a transparent background,
under the media feature guard and before activation. Failure to enable alpha
leaves construction through the existing CleanupStack path; it is not ignored.
The display-mode return value is not mistaken for a KErrNone error code.

Opened native video takes the existing software-painting fallback. EGL page
composition is disabled, its context/surface is destroyed, and the entire
bitmap is repainted before drawing the native-video hole. This is not a claim
that the MMF surface is composited through a live GPU page surface.

The 12-second test deadline applies to awaited play Promise settlement after
metadata is available. Playback position, pause stability and frame/PCM checks
remain separate requirements. The trace records request/resolution timestamps
and timeout state; a slow or stuck Promise remains a failure. Native fallback
or a passing API count alone is not accepted as video-pixel evidence.
