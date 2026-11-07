# Frame-rate benchmarks

Fixed pages for measuring presentation on a device with `frame-stats`. The
package installs them at `C:\Data\WebKit\bench`.

| Page | Measures | Expected presentation |
| --- | --- | --- |
| `scroll.html` | Scrolling a long text page, 6 px per frame | ScreenPlay (bitmap without it) |
| `images.html` | Scrolling 60 scaled PNG and JPEG images | ScreenPlay |
| `animation.html` | 16 elements moved with `left`/`top`: layout and repaint | ScreenPlay |
| `composited.html` | 8 layers animated with 3D transforms | EGL |
| `canvas.html` | 150 shapes redrawn per frame on a 2D canvas | ScreenPlay, or EGL if the canvas is composited |
| `webgl.html` | 200 triangles redrawn per frame | EGL |
| `paint.html` | The whole view repainted per frame over eleven kinds of content (text, shadows, rounded and translucent boxes, gradients, scaled images), 4 seconds each, logged as `paint-CASE` | ScreenPlay |

## Running on the phone

1. Create an empty file `C:\Data\WebKit\frame-stats` and start the browser.
   The log then shows `frame-stats: enabled`.
2. Open `file:///C:/Data/WebKit/bench/index.html` and choose "Run all". Each
   page runs for 15 seconds and stops by itself; the run ends on the index.
   Don't touch the screen while it runs.
3. Copy `C:\Data\WebKit\log.txt` and summarize it:

```sh
python3 Tools/Symbian/tests/benchmarks/summarize-frame-stats.py log.txt
```

For each page this prints the page's own `requestAnimationFrame` rate, and
for each presentation that drew frames: frames per second, milliseconds per
frame (the sum of the stage averages), the windows averaging over 33 ms per
frame, and each stage's average/maximum. The native `frame-stats` numbers
count frames actually presented and are the ones to compare; the page's rate
is only how often it was given a frame. `--compare before.txt after.txt`
prints the change per page and presentation, and `--json` the raw numbers.

By default every page is composited with EGL and scrolls cached GPU tiles,
so pages listed above as ScreenPlay show up as `egl`; "GPU tiles" is the most
tile memory the page used (the budget is 32 MB). To measure the ScreenPlay
path instead, also create an empty `C:\Data\WebKit\no-gpu-tiles` and restart
the browser.

## How the log is grouped

`bench.js` writes `bench: start NAME`, `bench: end NAME ...` and
`bench: skip NAME REASON` to the console, which the browser logs. The
summarizer groups `frame-stats` lines between start and end under the page,
and drops each presentation's first line in a group: it carries the previous
page's last frames and the idle second before the start.

`test-summarize.py` checks the summarizer on a synthetic log and runs in
`run-smoke.sh`.
