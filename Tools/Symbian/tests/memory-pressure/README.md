# Native memory pressure checks

`run-host.sh NEW_DIRECTORY` compiles the production pressure policy and the actual
native observer against a small SDK mock. The policy checks thresholds,
hysteresis, notification floods, cooldown, holdoff and unavailable HAL values.
The observer checks failures, rearming, cancellation, handle closure and the
native allocation/deletion pairing. These tests do not establish real native
notification delivery or JavaScript object retention during GC.

`run-eka-host.sh EKA2L1_SOURCE NEW_DIRECTORY` compiles EKA's actual
`change_notifier.cpp`, with only its scheduler/request environment mocked, plus
the production accounting arithmetic. Checks include initial/coalesced events,
cancel/close, exited requesters, kernel teardown and a thread object address
reused with a different UID. Apply patch 0021 to that source first.

Build one native probe using the normal SDK/tool environment:

```sh
python3 Tools/Symbian/tests/memory-pressure/build-native.py \
    --sdk /path/to/symbian-sdk --output /new/build-directory --mode baseline
```

Use `--mode threshold` for the second probe. The wrapper snapshots the actual
production bridge, builds with one job, and records source/image hashes. Both
programs require no arguments and return a unique `EXIT error=N` line. Run only
one probe at a time, with fresh logs, a 20-second external watchdog, and normal
guest/host termination checks. The probes do not modify system thresholds.

| Mode | Guest log | Scope |
| --- | --- | --- |
| baseline | `C:/Data/memory-pressure-probe.log` | Reserve/commit/release 64 MiB, HAL accounting, page patterns, native observer lifecycle. `accounting=UNAVAILABLE` confirms a limitation; it is not overall success. |
| threshold | `C:/Data/memory-pressure-threshold.log` | EKA's explicitly declared 8/24 MiB notification defaults, actual RChunk commits in 4 MiB steps (248 MiB hard cap), low/good crossing, coalescing, cancellation and at least 80 MiB of live page patterns. |

The threshold test stops if HAL does not decrease with real commits. It is an
EKA policy test, not a promise that every phone uses 8/24 MiB native thresholds.
It must report all six `PROBE_RESULT` fields as PASS and `EXIT error=0` without a
panic. Its live pages are native RChunk pages; browser JS roots and cache relief
require a separate integrated browser test.

The host tests never open XDG state or launch EKA. See the
[validation evidence](../../docs/validation-belle/memory-pressure/README.md).

## Integrated browser retention

Serve this directory over local HTTP, then load
`browser-live.html?run=UNIQUE_32_HEX_TOKEN`. The page retains 1,572,864 linked
objects and an 8 MiB buffer, checks every cell and every buffer page repeatedly
for 40 seconds, and redraws/reads a canvas after cache relief. It never invokes
a GC or memory-pressure testing API. The native callback must separately log at
least 80 MiB of actual JSCell bytes; allocation counts are insufficient evidence.

Build `--mode browser-worker` and launch it as a second guest process in the same
EKA instance, preserving the browser's ordinary launch/exit identity. Its log is
`C:/Data/memory-pressure-worker.log`. Once the page's unique READY is observed,
the runner writes that token plus newline to `C:/Data/memory-pressure-start`.
The worker waits at most 60 seconds for that file, commits real pages until EKA's
low threshold is crossed (248 MiB hard cap), holds them for 12 seconds, checks its
own patterns and releases them. It does not send a message or signal to WebKit.

`validate-browser.py --browser-log FILE --worker-log FILE --token TOKEN
--script-url http://localhost:PORT/browser-live.js` requires both actual native
pressure and actual JS heap measurements between page READY and later complete
live-data verification. A page PASS alone, buffer bytes mislabeled as cells,
synthetic/no-native relief, missing later verification and a release storm fail
the validator. `test-validator.py` exercises those rejection contracts.

The outer runtime runner must additionally enforce fresh logs, exact binary and
fixture hashes, exclusive XDG locks, finite deadlines, no panic, and normal host
termination. Delete any stale start file before launching either guest process.
