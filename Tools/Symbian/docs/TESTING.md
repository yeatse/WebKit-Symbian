# Testing

Run commands from the checkout root. Build setup and package commands are in
[Development](Development.md); module coverage is in [Progress](PROGRESS.md).

## Host smoke suite

Requires Python 3 and a C++20 host compiler.

```sh
bash Tools/Symbian/tests/run-smoke.sh
```

The suite exercises memory allocation, filesystem operations, IndexedDB quota,
cookie persistence and policy, ScreenPlay buffers and Surface Update lifecycle.
It finishes with `PASS: Symbian host smoke tests`.

## Emulator setup

Install Symbian^3 or Belle firmware in an EKA2L1 profile. Use that profile's XDG
paths and an active X display:

```sh
export SYMBIAN_WEBKIT_WORK=/path/to/symbian-work
export EKA2L1=/path/to/eka2l1_qt
export XDG_DATA_HOME=/path/to/eka-profile/data
export XDG_CONFIG_HOME=/path/to/eka-profile/config
export XDG_CACHE_HOME=/path/to/eka-profile/cache
export XDG_RUNTIME_DIR=/path/to/eka-profile/runtime
export EKA2L1_DATA="$XDG_DATA_HOME/EKA2L1/data"
export DISPLAY=:99
```

Use the browser executable alongside `browser_reg.rsc`, `browser_loc.rsc` and
`browser_ui.rsc`. The browser driver stages resources, ICU data and test pages
into the profile. Set `EKA2L1_DEVICE` when selecting a device other than RM-707.

## JavaScript

Run the language and internationalization cases with the JSC driver, which copies
`jsc.exe` and `Tools/Symbian/tests/*.js` into the emulator:

```sh
Tools/Symbian/eka2l1/run.sh C:/Data/basic.js C:/Data/es2020.js \
  C:/Data/intl.js C:/Data/resizable-arraybuffer.js
```

Each script prints its PASS line; the driver also checks successful process
completion and saves logs under `$SYMBIAN_WEBKIT_WORK/runs`.

## Browser graphics

The default browser presents ordinary pages through ScreenPlay and composited
pages through EGL, so one build runs compositing, WebGL and ScreenPlay:

```sh
python3 Tools/Symbian/tests/run-browser-graphics-tests.py \
  "$SYMBIAN_WEBKIT_WORK/build/browser/e32/browser.exe" \
  --suite all --output /tmp/graphics-results
```

The browser's switches are in `C:\Data\WebKit\webkit.ini`, which it writes
with every setting at its default, commented out, when the file is missing;
remove the `#` before a name, edit the value and restart the browser. The log's `settings:` line names the ones not at their
default.

| Setting | Default | |
| --- | --- | --- |
| `frame-stats` | off | frame timings, main-thread profile and decode times in `log.txt` |
| `profile` | off | samples the UI thread's program counter every 5 ms while it runs, for up to ten minutes of running, and writes `profile:` lines to `log.txt`: after every 60 s of running, with addresses seen once counted by 4 KB, and in full when the browser's own Exit command closes it. Name them with `python3 Tools/Symbian/tests/benchmarks/symbolize-profile.py log.txt browser.elf`, `browser.elf` being the `elf` artifact of the same CI run |
| `curl-verbose` | off | curl's own trace in `curl.txt` |
| `gpu-tiles` | on | composite every page with EGL; off keeps ordinary pages on ScreenPlay |
| `composited-video` | on | video in a window under the page, keeping EGL while it plays |
| `backdrop-filters` | off | draw CSS `backdrop-filter` (about 10 ms a frame on a Nokia 808) |
| `timer-deferral` | on | hold page timers while the user touches or scrolls |
| `jit-priority` | on | JIT compiler threads at the UI thread's priority; off puts them below it |
| `js-tier` | dfg | `baseline` leaves out the DFG compiler, `llint` runs the interpreter only |
| `js-options` | | JavaScriptCore options, as `thresholdForOptimizeAfterWarmUp=2000 useDFGJIT=false`; logged on a line of its own |

Add `frame-stats` before the URL on the browser command line, or set
`frame-stats = on`, to log `frame-stats:` lines with average/maximum
milliseconds per stage about once a second: `update`, `backing`, `draw` and
`swap` for EGL, `begin`, `paint` and `submit` for ScreenPlay, `paint` and
`blit` for bitmaps. Compare them on a device; emulator GPU timings use the host.

The package installs fixed benchmark pages (scrolling text and images, layout
and composited animation, canvas 2D and WebGL) at
`file:///C:/Data/WebKit/bench/index.html`. Each runs for 15 seconds and stops;
"Run all" runs them in turn. Summarize the log per page and presentation, or
compare two runs:

```sh
python3 Tools/Symbian/tests/benchmarks/summarize-frame-stats.py log.txt
python3 Tools/Symbian/tests/benchmarks/summarize-frame-stats.py --compare before.txt after.txt
```

See [Benchmarks](../tests/benchmarks/README.md).

Each output directory contains `result.json`, browser and emulator logs, and a
PNG screenshot for each suite. Page checks cover layout, painting and animation;
the runner checks native surface presentation. See
[Browser graphics](../tests/browser-graphics-runner.md) for individual suites
and fixture screenshots.

## Browser services

Start the local fixture server described in each guide, then run its browser
suite. Network and media runners accept `--data` for a specific profile and
`--output` for a new result directory.

| Area | Main flow |
| --- | --- |
| [HTTP and TLS](../tests/network-tests.md) | Requests, redirects, origin checks and certificates |
| [Audio and video](../tests/media.md) | Open, play, pause, seek and completion |
| [Storage](../tests/belle-storage/README.md) | Persist data, reload and read it back |
| [Clipboard](../tests/belle-clipboard/README.md) | Copy and paste through the native clipboard |
| [Touch](../tests/belle-touch/README.md) | Tap, drag and scroll |
| [Browser UI](../tests/belle-ui/README.md) | Navigation, text input, file selection and exit |

Open a standalone page with the browser driver:

```sh
Tools/Symbian/eka2l1/run-browser.sh \
  "$SYMBIAN_WEBKIT_WORK/build/browser/e32/browser.exe" \
  file:///C:/Data/WebKit/webcrypto.html
```

The [test index](../tests/README.md) links the focused host and native-service
guides.
