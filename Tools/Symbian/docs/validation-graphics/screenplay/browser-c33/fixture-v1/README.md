# Offline ScreenPlay browser page fixture

This is a new standalone fixture for the C31 forced-ScreenPlay browser. It has not been run in EKA or a browser. The shared WebKit repository and all old tests/evidence remain untouched. `runtime-metadata.json` identifies C31 as `BUILD_AND_LINK_PASS_RUNTIME_PENDING`, with runtime/display/phone verification false. The parent integration task reports that the latest EKA v3 change maps a pending bounded Finish to NotReady; that integration is not independently validated by this package. This fixture does not establish complete NativeFullFinish semantics or an end-to-end ScreenPlay connection.

`screenplay.html` is self-contained, loads no network resources, reads no cookies/storage/profile/ROM/clipboard, and logs only fixed public records. It paints opaque primary-color patches, fixed-size ordinary text, overflow clipping, and 50% red over white; schedules three small dirty-region changes; requests a CSS move; and changes a DOM box width 48 → 80 → 64 CSS pixels. Canvas 2D and a WebGL clear/readback are explicit optional scopes. Neither offscreen readback nor successful DOM geometry is display proof. This is an adaptation of the public compositing/text/WebGL fixtures pinned in `source-pins.json`.

The canonical target is:

```text
file:///C:/Data/WebKit/screenplay.html?run=<fresh-32-lowercase-hex>&canvas2d=1&webgl=0&tap=0
```

The query order is intentional and strict. `canvas2d`, `webgl`, and `tap` each accept only `0` or `1`. A missing/invalid query leaves the page inactive with a fixed configuration message and no console record. The outer controller must generate a new token for every run, bind it to the actual launch and preserve the original console log; the parser cannot establish freshness from a copied record alone. `check.py url --run TOKEN` prints the canonical URL without opening anything. Use `--canvas2d 0`, `--webgl 1`, or `--tap 1` consistently in both commands when changing scope.

The controller may provision this single public HTML file at `C:\Data\WebKit\screenplay.html`. This package does not install it, launch anything, acquire namespaces, inspect private configuration, or change SDKs. Root owns the preserved namespace and runtime evidence.

Normal work completes at about 2500 ms; `tap=1` keeps the optional button observable until about 8500 ms. A 9000 ms watchdog requests a terminal failure. JavaScript timers cannot guarantee scheduling while a guest is suspended; the parser rejects terminal elapsed time greater than 10000 ms. The outer controller must enforce its own bounded observation. The fixture never closes the window, requests EndTask, dispatches synthetic input, or replays a crash.

The sole console label is `SYMBIAN SCREENPLAY ` followed by flat JSON. The existing public `SymbianChromeClient` adds `console: <complete URL>:<line>: `. `record.schema.json` specifies exact record fields and fixed enums with `additionalProperties:false`. `check.py` also enforces sequence, identity, exact scope, each named check exactly once, three ordered dirty requests, optional-scope rules, counts, one terminal record, and the duration bound. Unknown public fields and labels are rejected without echoing their values. All same-label records from stale URLs or without the exact wrapper are rejected. Unrelated native diagnostics are not interpreted as page evidence.

For a console log collected by Root:

```sh
python3 check.py check --run TOKEN --log /path/to/public-console.log
```

Exit code 0 means `PAGE_REQUESTS_PASS` only, 1 means a well-formed page failure, and 2 means malformed/incomplete/rejected page records. Every accepted report fixes display and trusted input to `UNVERIFIED`, requires separate runtime review, and marks NativeFullFinish unproven by the page. Native `ScreenPlay: surface submitted` text is separate path evidence and is insufficient to prove displayed pixels. No global render PASS is produced here.

`manifest.json` contains fixed screenshot patch ROIs in page viewport CSS coordinates. They are interiors of flat patches, avoiding borders, glyphs, antialiasing and element edges. The caller must independently identify the browser content origin, CSS-to-screenshot scale and owned current window before mapping those ROIs. Screenshot comparisons should wait for the terminal page record and use the original captured PNG, record its identity, the mapping and measured pixels. The optional font sample has fixed size and content for visual review only; exact glyph pixels depend on the public font build. Public own-color readback in canvas scopes cannot replace screenshot evidence.

The optional button is at viewport CSS rect `(16,336,224,40)`, center `(128,356)`. `hit_test` is only `elementFromPoint` geometry. With `tap=1`, the page records at most one click's `isTrusted` and a center hit test; a trusted click can also originate from keyboard/platform behavior, so this record alone never proves RootTrustedInput or a physical tap. Root must separately correlate real input, process/window identity and the event. Input is currently **NOT READY** for this fixture's runtime workflow: installed XRes headers are absent, and `_NET_WM_PID=2` is not a reliable PID-namespace ownership match. Do not infer ownership or inject input from that value. This package performs no input or ownership checks.

Run only the host checks here with:

```sh
python3 test_host.py
```

They use synthetic records, static offline checks and JavaScript syntax parsing. They do not execute this page or provide browser/display/input evidence. `host-checks.txt` and `host-validation.json` record that limited validation. `artifact-pins.json` binds the delivered files; `source-pins.json` binds the actual public source bytes (the checkout has existing WIP, so HEAD alone is insufficient).
