# Browser graphics validation

Use the installed firmware's XDG environment, an X display, and a complete browser image with its adjacent resource files. The runner obtains both existing emulator locks and invokes the standard browser driver sequentially. It does not start an HTTP server or install a test CA; it checks that the existing CA bundle stays unchanged.

```sh
export SYMBIAN_WEBKIT_WORK=/path/to/graphics-work
export EKA2L1=/path/to/eka2l1_qt
export DISPLAY=:99
python3 Tools/Symbian/tests/run-browser-graphics-tests.py \
    /path/to/e32/browser.exe --suite all --timeout 60 \
    --data /path/to/EKA2L1/data --output /path/to/new-evidence-directory
```

`--suite compositing` and `--suite webgl` select one page. Optional `--browser-sha256` and `--emulator-sha256` pin the expected executable identities; both actual hashes are always recorded. `--timeout` is the observation period per page, followed by bounded screenshot and graceful shutdown work. The output directory must be new.

Acceptance requires a fresh per-run console URL, each expected named check exactly once, one successful terminal result (compositing 10/10 and WebGL 9/9), no panic, native EGL/GLES2 initialization followed by a successful native presentation, no compositor fallback, a PNG screenshot, and successful browser-driver shutdown. Failed runs retain available raw logs and screenshots. `result.json` records page checks and presentation diagnostics separately. The graphical browser does not emit a probe-style guest exit marker, so the report records the driver's observation and graceful host shutdown rather than claiming a standalone guest exit code.

Review `compositing/browser.png` against [compositing.md](compositing.md) and `webgl/browser.png` against [webgl.md](webgl.md). The runner verifies screenshot presence and dimensions, not image similarity. The compositing page changes layout width; native surface resize, input-panel overlay, navigation and software fallback still require separate observations. Page success does not prove hardware composition; native presentation logs supply separate path evidence. EKA validates guest graphics dispatch, not a physical device's GPU or the JavaScript execution tier. Pair browser evidence with the independent EGL/ANGLE probes and preserve their executable identities.

The [2026-10-29 browser record](../docs/validation-graphics/browser-final/README.md) passes both suites on browser `8ba0a506…` and host `ff218e2e…`, with full hashes and original logs/screenshots retained. Its separate [text/stripe comparison](compositing-text.md) verifies the promoted-text regression that the geometry checks alone could not detect. Browser media acceptance is a separate pending task.

Host-only negative checks, without EKA or shared XDG access:

```sh
python3 Tools/Symbian/tests/test-browser-graphics-runner.py
```

These fixtures exercise stale runs, wrong/duplicate results, missing native presentation, software fallback, panic, missing screenshots, nonzero driver exit, timeout cleanup, and unchanged CA bytes. They validate the runner; they are not browser rendering results.
