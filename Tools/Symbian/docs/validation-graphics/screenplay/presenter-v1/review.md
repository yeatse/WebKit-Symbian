# Independent presenter review

Status: no remaining blocker found in the frozen V1 presenter source.
The exact identities and host test limits are in `result/validation.json`.

The implementation requests two mappable premultiplied-ARGB buffers, validates
the actual size/format/count/stride and each returned buffer offset against the
mapped chunk, and rejects overlap. Cairo can write into an available mapped
buffer directly. The per-slot damage and separate pending-output damage avoid
both stale slot reuse and unnecessary buffer flips on an ordinary exposure.
Paint starts on white and transparent operations are flattened over white
before cache synchronization because the background surface contract is opaque.

`NotifyWhenAvailable` is registered before submission. A busy pair defers without
reusing pixels, and the availability AO requests another redraw. The state
machine treats notification errors as a fallback condition. Detachment removes
only its own current background, finishes that WServ command, cancels/drains
statuses, unregisters, then closes the mapping/surface. Cancellation is not
treated as permission to reuse a submitted buffer. Neither this sequence nor
`Finish` is claimed to prove scanout/vsync completion.

Review findings resolved before V1 freeze:

- Retire the background before cancelling and draining notifications.
- Stop a resize after retirement records a WServ error.
- Reject invalid width/height before computing the buffer-size product.
- Do not wait for a never-armed status during partial construction or teardown.

Native integration requirements identified and communicated to Root:

- `GetBackgroundSurface` is not legal on an alpha-transparent window under the
  formal WServ contract. The shell must begin opaque and detach/delete the page
  presenter before enabling alpha for native MMF video. That transition must
  permanently disable this presenter for the same window; `SetNonTransparent`
  is a deprecated no-op and is not a valid inverse.
- The MMF factory must call the shell transition before `AddDisplayWindowL`,
  including the early native callback path before the next browser redraw.
- That early hook should also retire any EGL compositor that still owns this
  window, before MMF can replace its background surface.
- Clearing retained GC content should use `RWindowBase::KeyColor`, since the
  screen may select alpha or chroma-key composition.
- Keep the bitmap fallback lazy, and require a complete repaint after a surface
  failure or native-video transition.

The revised Root overlay inspected implements the opaque initial window,
permanent MMF transition, pre-AddDisplayWindow callback, KeyColor and lazy bitmap
fallback. Root owns final source reconciliation and actual ARM/native/browser
validation. The host adapter does not independently prove those window-system
contracts or their execution in the whole browser.
