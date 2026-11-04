# ScreenPlay probe v2 API review

This is a read-only review of the current protocol-v2 source and parser. The reviewer did not compile or execute code, synthetic tests, a guest, an emulator, a display or a phone.

## Correction of the v1 review

The previous v1 review missed a blocking Window Server precondition. It approved the normal path without checking the NGA client implementation's full `GetBackgroundSurface` contract. v1 enabled window alpha transparency and then called `GetBackgroundSurface`; the formal contract expressly prohibits that combination and documents `EWservPanicTransparencyMisuse`. The former “no confirmed blocker” conclusion for v1 was wrong and must not be used as permission or evidence to run it.

Root identified this before guest execution. The unchanged old artifact and old review are preserved under `/workspace/symbian-screenplay-probe-v1-superseded-20261102`, with `SUPERSEDED.json`. This current report does not rewrite that failed review as a success.

## Formal window contract checked for v2

The fixed upstream NGA source was read directly, including `CLIENT/RWINDOW.CPP` lines 1640–1885:

- `SetBackgroundSurface(configuration, ...)` requires the surface to be opaque and valid configuration members. v2 fills every tested surface pixel with one of two fixed ARGB values whose alpha is 255; it does not request semi-transparent surface composition.
- `GetBackgroundSurface` explicitly requires that the window has not been made transparent. v2 creates its own fresh RWindow and does not call `SetTransparencyAlphaChannel`, other transparency enabling APIs or `SetSurfaceTransparency` before either Get call. Its `opaque_window` record is a source/control-flow assertion, not an independent runtime query.
- `SERVER/openwfc/WINDOW.CPP` initializes ordinary window flags with `EFlagHidden`. Its `SetRequiredDisplayModeL` only returns the screen display mode. `SERVER/openwfc/cliwin.cpp` handles the separate `SetTransparencyAlphaChannel` opcode by setting `EFlagHasAlpha`. These paths confirm that an alpha-capable pixel/display mode request and enabling window transparency are distinct operations. They do not relax the published opaque-window precondition; no claim is made that every inspected server version enforces it with the same conditional.
- NGA `SetRequiredDisplayMode` is deprecated: the client source says the argument is ignored and the current system display mode is returned. v2's `LeaveIfError` use correctly accepts a nonnegative display-mode return. The call does not prove that the actual mode has alpha; composition mode remains system-selected.
- `KeyColor()` returns the system's key for a window with a surface, and transparent black when there is none. v2 calls it only after Set/Get succeeded and ownership was confirmed, then uses that returned color for the window background and a full GC overlay clear. It does not assume transparent black or expose the actual color in the public record.
- Get is documented to return equivalent configuration attributes, rather than arbitrary byte-for-byte copies. The probe compares the owned ID and its explicit simple extent/viewport fields only. The examined server path restores explicit original viewport and window-relative extent; the probe does not compare whole configuration object bytes.

The former transparency contract violation is removed in the current v2 source. No remaining blocking violation was found in the examined normal v2 API path; this is a source-review result, not native compatibility or presentation evidence.

## Buffer, notification and cleanup contracts

Both buffers are filled before either is submitted. Actual SurfaceInfo, GetBufferOffset and mapped chunk bounds are used, and SynchronizeCache(ESyncBeforeNonCPURead) precedes each submission. Only boolean bounds/pixel outcomes enter the public record.

Each long-lived request status is registered with NotifyWhenAvailable before its associated SubmitUpdate. The second buffer is submitted without waiting for the first one's Available, because a compositor may retain its current buffer until replacement. Displayed is never treated as permission to write. The first successful Available is required after the two submissions; the final notification may be canceled during teardown.

Teardown checks the currently attached surface ID, removes only the owned background, checks Finish, cancels and drains notifications, unregisters and checks Finish, closes the surface reference, then closes the mapping. A drain timeout retains request storage/handles until process teardown and prevents AVAILABLE. Native synchronous IPC calls still need Root's outer deadline, health and restoration gates.

The local CBase deleting destructor matches User::Free. The window wrapper allocations use checked nothrow portable new and matching delete. A trapped cycle timeout sets the per-cycle and aggregate timeout flags.

## Public protocol boundary

The current parser accepts version 2 only. `alpha_rc` was removed; result `opaque_window` and per-cycle `overlay_uses_key_color` were added. Successful records require the opaque-window assertion and the actual-key overlay assertion. The new failure guards reject omission of either condition.

The parser also bounds the file and line count, refuses symlinks/nonregular files, rejects duplicate/unknown fields and nonfinite values, binds every record to the supplied run token, checks primitive types and cycle order, and cross-checks submission/completion/cleanup facts. AVAILABLE requires both complete size cycles, four submissions and complete non-retained cleanup. It never equates syntax acceptance with runtime origin.

The inspected file defines 34 synthetic test methods. The preparation agent reports its synthetic checks and ARM/E32 build passed; this reviewer did not rerun them. Synthetic tests only validate record boundaries. Mapped pixel bytes, SubmitUpdate, Available, Finish and compilation do not prove screen refresh, vsync, performance, zero-copy, browser integration or phone compatibility. Those remain pending independent runtime checks. display_verified/vsync_verified/performance_verified remain false.

## Reviewed identities

- `screenplay_probe.cpp`: `2695362b3603fac109c8a4db6b62ae5ab09ce2b1f952d395e5512c2ce9d83b81`
- `validate.py`: `e5da8a8f1f3363e9257330c025e780b82b9567d4950725b2411ccd34eeb8afcc`
- `test_validate.py`: `cc520c42e0603c415668a5912c44c52e4e3880e777458d37ed914a62d911b2d1`

Fixed upstream sources inspected:

- `windowing/windowserver/nga/CLIENT/RWINDOW.CPP`: `63f89f297e845494f97a024cb85ee2572302794fb54b19afc150565f354cb15d`
- `windowing/windowserver/nga/SERVER/openwfc/WINDOW.CPP`: `58d116c801dd28939670f2c7f5d209a5890d589d106deef9023ea0934d5d45de`
- `windowing/windowserver/nga/SERVER/openwfc/cliwin.cpp`: `09aa701ed5660effe7eb30b97b36288169078edf2bf81972065e514f41d64219`
- `windowing/windowserver/nga/SERVER/openwfc/wnredraw.cpp`: `1098f217850d9b6c539f643be4fc16e4720d4bb4f25b6c663c2be319f2d80edc`
- `graphicshwdrivers/surfacemgr/src/surfacemanager.cpp`: `dfb00be6faf8146a116efe2a79849806b2923a39986ea3f1ef13144c3dd92a30`
- `graphicscomposition/surfaceupdate/src/surfaceupdateclient.cpp`: `3802ef161726fcd7d84e4912958bc53749759df9859703584cf6bacfb845aede`
