# ScreenPlay presenter host verification

The actual `ScreenPlaySurface.cpp` production translation unit is compiled with
the narrow API adapter in `fake-sdk/`. Its state machine and rendering-buffer
algorithms are not copied into the test. `tests.cpp` drives the public presenter
interface and deterministic owned API completions. The runner verifies both
production input digests again after execution.

```sh
python3 run.py --source-dir /path/to/Tools/Symbian/browser --output result
```

`result/validation.json` records the exact two source identities and 47 test
scenarios with 298 assertions. Compilation and execution passed with Undefined
Behavior Sanitizer enabled. `result/test.log` contains only fixed test labels
and the strict aggregate result. The fake API uses its own byte arrays, handles,
statuses and events; no guest, display, device, ROM or original user state is
opened by these tests.

The scenarios exercise padded actual-API stride and nonzero buffer offsets,
per-buffer accumulated damage, no-damage exposure, two pending buffers, bounded
deferral, availability callbacks, abort/stale frames, white-backed premultiplied
alpha flattening, foreign background preservation, resize/retirement, request
cancellation/draining, unsupported/error returns, invalid dimensions including
the signed-maximum boundary, and partial construction with never-armed statuses.

These are host functional checks. They do not verify the Symbian ABI, DLL
imports, the real active scheduler, cross-process lifetime, driver ownership,
cache coherency, visible display pixels, refresh/vsync, throughput or a phone.
Actual ARM compilation and native/whole-browser runtime remain separate Root
validation. The fake window deliberately represents an already opaque window;
the required native video transition is reviewed separately in `review.md`.

Only the explicitly listed source/report files in `manifest.json` are the
handoff. The local `tests` executable and `presenter.o` are disposable build
outputs and are not included in the public handoff manifest.
