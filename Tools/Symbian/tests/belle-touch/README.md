# Native touch bridge tests

Run `bash run-host-tests.sh` for allocation-free native contact snapshots and
the default gesture policy. The tracker checks all active contacts, stationary
points, release coordinates, primary-contact lifetime, identifier reuse,
cancellation and the configured native capacity. The gesture checks cover
default prevention, pan axes, movement beyond the tap threshold, cancellation
and suppression of compatibility clicks after multiple contacts.
Extreme coordinate deltas are saturated without signed overflow. Cancelling
`pointerdown` suppresses compatibility mouse events without disabling native
panning; cancelling `touchstart` or `touchmove` suppresses both.

These are host unit tests for the same helpers intended for the native bridge.
They do not prove DOM event delivery, Pointer Events, navigation cancellation,
native capability queries or real-device multitouch. The helpers are initially
added independently of the current browser candidate; an existing executable
does not gain touch support merely because these files exist.

Capability reporting must use the digitizer's presence and the enabled pointer
count (`HALData::EPen` and `EPointerNumberOfPointers`), bounded by the embedding's
supported storage. `EPointerMaxPointers` is a hardware/driver maximum, not proof
that those pointers are enabled for this window. `EnableAdvancedPointers` must
precede window activation. Legacy devices whose pointer-count attribute is
unsupported may expose one pointer only when the digitizer is present, as
specified by the native `TAdvancedPointerEvent::PointerNumber` contract.

EKA currently advertises eight digitizer pointers. A future XTest single-finger
run is evidence only for the exercised single-finger behavior. Any separate
WS advanced-pointer injection must be labelled as injected native events,
never as a physical eight-finger device test.

The default browser gestures in this stage are single-finger panning, taps and
the existing double-tap zoom. Native pinch zoom is not implemented. Delivering
multiple DOM contacts allows a page to implement its own gesture; it does not
establish a browser pinch-zoom feature.

## Ordinary page fixture

Serve this directory, for example with
`python3 -m http.server 8783 --bind 127.0.0.1 --directory Tools/Symbian/tests/belle-touch`,
then open `http://localhost:8783/?case=tap&run=UNIQUE_ID`. Each case requires
fresh native input and finishes with `SYMBIAN TOUCH CASE DONE PASS|FAIL` and
the actual event trace. Missing completion is failure. The fixture rejects
untrusted script-created events and uses no production testing hooks.

| Case | Required native interaction and check |
| --- | --- |
| `tap` | One tap: one touch start/end, pointer down/up, and compatibility mouse/click sequence. |
| `prevent-start` | One tap: `touchstart.preventDefault()` suppresses compatibility mouse/click. |
| `prevent-move` | Upward drag of at least 30 px: cancelled touchmove prevents native scrolling and clicks. |
| `pan` | Upward drag: overflow scroll position changes; touch/pointer streams cancel exactly once. |
| `pointer-pan` | Same drag with cancelled pointerdown: compatibility mouse stays suppressed but native scrolling still works. |
| `none` | Upward drag with `touch-action:none`: DOM move/end continues, without native scroll/click. |
| `capture` | Press in panel, drag beyond its right edge, release: pointer capture retains events and is released. |
| `cancel` | Hold contact, independently remove native focus/cancel contact: cancellation, no up/click. This needs a real native action or explicitly labelled WS injection. |
| `navigation` | Press and hold: page navigates after 250 ms, and the landing page verifies the old page's cancellation record for this unique run. |
| `multi` | Two simultaneous native contacts then release: complete touch lists, distinct pointer IDs, one primary contact, and no extra stationary pointermove or mouse click. |

Keep the screenshot, event trace, browser/host identity, and native input
actions for each case. Do not count the presence of `ontouchstart`, a positive
`maxTouchPoints`, or a `READY` log as a pass. `navigation` ends on the `landing`
case; retain both page logs. A host-only JavaScript syntax check does not
validate any of these DOM or native behaviors.
