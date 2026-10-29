# Natural-end event ordering

The LLInt diagnostic reached all HTTP groups without the earlier access
violation, but failed the original natural-end assertion: 13/14 remains a
failed historical result. The test polled `element.ended` and immediately
required its event counter to equal one. Its `finally` block then called
`load()`, which could cancel the still-queued event.

In this WebKit source, `HTMLMediaElement::ended()` and `endedPlayback()` derive
the property from the playback position (`HTMLMediaElement.cpp`, lines 4373
and 6370). `mediaPlayerTimeChanged()` schedules `ended` at line 5914;
`scheduleEvent()` at line 1292 queues a cancellable `TaskSource::MediaElement`
task. `prepareForLoad()` cancels these pending tasks. This matches the HTML
[playing-the-media-resource event ordering](https://html.spec.whatwg.org/multipage/media.html#playing-the-media-resource):
reaching the end queues the event; it does not synchronously dispatch it.

The revised fixture waits for the actual event count to become nonzero, or for
an error, within the original seven-second deadline. It then requires exactly
one event, `ended`, `paused`, no pending seek, and a playback position within
150 ms of duration. Missing events time out and duplicate events fail. No
production backend state or callback was changed for this fixture correction.

Node syntax checking passed. A host execution of the actual test callback
accepted an ended property that preceded its event, and rejected a missing
event at 7000 ms, duplicate events, a media error, an unpaused element, a wrong
position and an outstanding seek. This checks the test's ordering and failure
conditions; only a fresh guest run can establish the backend event result.

Corrected `media.html` SHA-256:
`e46cf9432c9a26d8394ee59dab6b69590664d1b87135f33bfcbc29de87a070e8`.
