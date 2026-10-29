# LLInt diagnostic: historical FAIL, 13/14

This run uses the `89558145...` diagnostic image with JIT, Baseline, DFG,
RegExp JIT and DOM JIT disabled, as recorded in `console.log`. It reaches all
14 HTTP groups without an access violation. The old natural-end test reports
`wrong end/error event`, so the page and strict runner remain **FAIL**.

The page report and event trace are retained. They are not rewritten after
the [event-ordering fixture correction](../natural-end-fixture.md). A later
test fix cannot establish that this historical run dispatched an event which
its cleanup could cancel. The launch did not record a fixture-file hash.

The eight named PCM control windows pass the unchanged independent analysis.
Its full raw-capture SHA is retained, but the recording and screenshots remain
external. Those observations do not turn this diagnostic into JIT or complete
media acceptance. `evidence.json` records source-run identities and hashes;
host paths in launcher/result metadata are made portable.
