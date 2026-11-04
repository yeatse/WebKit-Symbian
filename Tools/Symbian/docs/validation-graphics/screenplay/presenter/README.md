# ScreenPlay presenter host verification

The actual `ScreenPlaySurface.cpp` and `.h` production files are copied byte for
byte into a private temporary source directory, then compiled with the narrow
API adapter in `fake-sdk/`. Their identities are checked before copying and
after execution; the copied identities are checked before and after execution
as well. The presenter's state machine and buffer algorithms are not copied
into the test. `tests.cpp` drives its public interface and deterministic API
completions. The temporary directory is removed when the runner exits.

```sh
python3 Tools/Symbian/tests/screenplay/run.py --output /tmp/screenplay-results
# To verify another production-source checkout:
python3 run.py --source-dir /path/to/Tools/Symbian/browser --output result
```

`result/validation.json` records the exact two production source identities and
47 scenarios with 298 assertions. Compilation and execution use Undefined
Behavior Sanitizer. `result/test.log` contains fixed test labels and the strict
aggregate result. The fake API uses its own byte arrays, handles, statuses and
events; no guest, display, device, ROM or original user state is opened.

The temporary source directory deliberately contains no real
`ScreenPlayUpdateSession.h`. That quoted include resolves to the
`fake-sdk/ScreenPlayUpdateSession.h` mock alias instead. This suite exercises
the presenter's calls and callbacks; it does **not** compile or validate the
private Surface Update client's implementation, protocol or server version.
Those checks belong to the separate private-client suite.

The scenarios exercise padded actual-API stride and nonzero buffer offsets,
per-buffer accumulated damage, no-damage exposure, two pending buffers, bounded
deferral, availability callbacks, abort/stale frames, white-backed premultiplied
alpha flattening, foreign background preservation, resize/retirement, request
cancellation/draining, unsupported/error returns, invalid dimensions including
the signed-maximum boundary, and partial construction with never-armed statuses.

These host checks do not verify the Symbian ABI, DLL imports, the real active
scheduler, cross-process lifetime, driver ownership, cache coherency, visible
display pixels, refresh/vsync, throughput or a phone. ARM compilation and native
and whole-browser runtime require separate validation. The fake window represents
an already opaque window; native-video window transitions are outside this suite.
