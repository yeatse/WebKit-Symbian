# Ordinary browser monotonic clock and timer acceptance

This fixture uses only `performance.now()`, eight sequential `setTimeout(40)`
callbacks and ordinary page logging. No native call, clock override, GC/testing
hook, injected event or precision bypass is used. It is independent of the
memory-pressure fixture and changes none of its 80MiB/40s/110s requirements.
No browser/emulator/display was started while preparing this directory.

The page records 64 synchronous monotonic reads and nine reads bounding the eight
timer yields. Samples must be finite, nonnegative and nondecreasing across both
phases; every timer must advance the clock. At least six of eight deltas must be
positive and less than 750ms, and the entire sequence must take less than 6s.
These bounds allow coarse NTick/fallback tick resolution and two large VM
scheduling delays. They do not require exact 40ms delivery, 10ms precision,
fractional milliseconds, or any particular output of the fingerprinting policy.
The earlier behavior of every 40ms timer taking one second fails both criteria.

WallTime, Date.now(), currentTimeInNanoseconds and suspend/deep-sleep semantics
are outside this fixture. Passing the page does not prove them corrected.

## Root runtime procedure

Use the actual final C15 image and adjacent resources after root pins their
identities. This directory intentionally does not invent a candidate15 SHA or
reuse C13/C14 evidence. Use a fresh 32-lowercase-hex run token and evidence
directory, the normal exclusive runner locks and a loopback-only HTTP server.
A server can serve this directory on an available local port such as 8804; no
server was started during preparation.

For the usual server layout:

- page URL: `http://localhost:ACTUAL_PORT/index.html?run=ACTUAL_32HEX_RUN`
- script URL for validation: `http://localhost:ACTUAL_PORT/suite.js`
- exact terminal runner marker: `SYMBIAN CLOCK ACTUAL_32HEX_RUN DONE PASS`

Normal root runner command structure, with actual environment values filled:

```sh
WAIT=60 EKA2L1_COMPLETION_GRACE=5 \
EKA2L1_EXPECT_PASS="SYMBIAN CLOCK ${CLOCK_RUN} DONE PASS" \
  /workspace/WebKit-Symbian-graphics/Tools/Symbian/eka2l1/run-browser.sh \
  "$CLOCK_C15_IMAGE" "http://localhost:8804/index.html?run=${CLOCK_RUN}"
```

Pin the actual image/host/resources and fixture/validator bytes externally,
preserve fresh raw logs, and require normal runner/host exit. The terminal marker
alone is insufficient. Run the validator on that run's archived browser log:

```sh
python3 -B /workspace/symbian-clock-work/browser-fixture/validate.py \
  --browser-log "$CLOCK_ARCHIVED_LOG" \
  --script-url http://localhost:8804/suite.js --run "$CLOCK_RUN"
```

The validator recomputes deltas, monotonicity, counts and elapsed time from raw
samples; it does not trust the page's reported PASS, aggregate count or deltas.
It requires exactly one READY followed by one DONE PASS from the exact source
URL/token and rejects failure markers, missing/duplicate results, backwards,
zero-progress, nonfinite/bool samples or changed requested timer counts/delays.
Its PASS explicitly covers page checks only. The outer runner retains binary
identity and normal-exit responsibility. Scheduling delays beyond these broad
bounds are recorded as a failed timed acceptance run, not silently relabeled.

## Preparation checks

`node --check suite.js` passes without executing the page. Eight small host
validator contracts pass, including a valid 15.625ms-derived coarse clock,
tolerance of two 1.2/1.4s VM pauses, and rejection of every-yield-one-second,
backwards/zero samples, wrong URL/token, duplicate/missing/failing records,
forged totals/nonfinite values and contract changes. `host-validator-tests.log`
contains their raw output. Synthesized host records are parser tests only;
there is no guest browser PASS in this delivery.
