# Browser graphics

The runner opens the compositing, WebGL, Ganesh and Skia damage pages, checks their console
results and native presentation logs, and captures a screenshot. Configure EKA2L1 using
[Testing](../docs/TESTING.md).

```sh
python3 Tools/Symbian/tests/run-browser-graphics-tests.py /path/to/browser.exe \
  --suite all --output /tmp/graphics-results
```

Use `--suite compositing`, `--suite webgl`, `--suite ganesh`, `--suite skia` or
`--suite screenplay` to select one page. Ganesh checks GPU drawing, pixel
readback and Canvas 2D copies between GPU and CPU surfaces, and requires native
Ganesh Canvas and layer rasterization markers. The colored scene shows red
above blue, with a green stripe on the left; its two copies retain that layout.

The default `auto` browser uses EGL for the compositing, WebGL, Ganesh and Skia pages.
Use the `screenplay` presentation build for the ScreenPlay suite.

The Skia suite exercises transformed layers, opacity, clipping, a mask,
filters, Canvas 2D and WebGL, then moves, removes and resizes layers and
updates root content. It enables frame statistics and requires native Skia
composition plus full and partial damage evidence. The
[native compositor probe](skia-compositor-probe/README.md) checks the GPU
pixels for old-bound repair, preservation of unchanged regions and resized
target repainting.

Passing results include every page check, native surface presentation, browser
logs and a PNG screenshot. Review the screenshots using the
[compositing](compositing.md), [WebGL](webgl.md) and
[ScreenPlay](screenplay.html) fixtures. Each output directory contains
`result.json` and the selected suite's logs and screenshot.
