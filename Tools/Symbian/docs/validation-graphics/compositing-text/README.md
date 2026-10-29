# Promoted text and stale raster scale

The first accelerated browser passed `compositing.html`'s ten DOM/geometry checks and presented native EGL frames, but its promoted text and one-pixel stripes were visibly blurred. The unchanged `compositing-text.html` fixture compared ordinary and promoted text, with and without scrolling, including integer line heights. All images below use the same 360 × 532 browser viewport. Plain text/stripes were clear in both paths; only promoted content was blurred in the accelerated image.

Historical evidence:

- `before/`: accelerated browser run `browser-20261029T153215Z-cvj0dguf`.
- `software/`: software browser run `browser-20261029T153235Z-qp6rpsi5`.
- `diagnostic/`: instrumented browser run `browser-20261029T154716Z-hsrojmar`, image SHA256 `df343f9165993af9c11b5cb42a85535f460a82d46dcdd00110168328622fdb8e`.
- `after/`: repaired browser run `browser-20261029T155748Z-nqhsi_zg`, browser SHA256 `8ba0a5068a3dbd3aef616d71ae3b9f6d6b0a906c376c49fa4a9cc04685eaead5`, emulator SHA256 `ff218e2e687c183c993aad2245824354e726f4c8bbefdff9b093b604602e18b1`.

Diagnostic frame 12 records page/device scale 1, while five promoted backings still had raster scale 0.3673 and no dirty region. A 280 × 17 layer had a 102.857 × 6.245 logical raster (the actual earlier upload allocated 102 × 6 pixels) and was drawn with scale 2.72222. The ordinary root backing retained scale 1. The stale scale came from the initial implicit 980-pixel viewport before the meta viewport selected scale 1; `360 / 980` is 0.3673469.

`GraphicsLayerTextureMapper` inherited an empty `deviceOrPageScaleFactorChanged` implementation. The fix makes that callback mark the complete layer dirty, so its next update refreshes the backing scale, rebuilds affected tiles, and rerasterizes all content. It does not change texture filtering, fixture styles, or expected results. Temporary scale/bitmap/frame diagnostics were removed after the cause was established. The final source adds only the scale callback and its declaration; its ARM syntax check passed.

The repaired browser logged native EGL presentation and completed its observation with normal host shutdown. Visual review found all six text rows clear. `text-comparison.json` also compares the six unchanged 280 × 24 text rectangles against the software screenshot: every RGB channel now matches exactly. Before the fix, ordinary rows 1 and 3 matched, while promoted rows 2/4 differed in 4,470 channels each and rows 5/6 differed in 4,946 channels each. Coordinates and original screenshot SHA256 values are recorded with those measurements.

The saved `stripe-analysis.json` files examine the original screenshots without modifying them. The ordinary one-pixel row is exactly black/white alternating; the historical accelerated promoted row fails that check. Both the software and repaired accelerated images pass both rows, with zero channel error. Reproduce with Pillow installed:

```sh
python3 Tools/Symbian/tests/analyze-compositing-text.py \
    Tools/Symbian/docs/validation-graphics/compositing-text/after/browser.png --require-match
```

Coordinates default to this captured emulator window within a 1280 × 900 screenshot. For another display layout, supply the actual `--left`, `--plain-y`, and `--promoted-y`; keep the fixture viewport and width unchanged. This establishes the reported text/stripe regression is fixed in this EKA run; it does not claim all text or rendering is correct. The same browser subsequently passed the separate [10/10 compositing and 9/9 WebGL browser suites](../browser-final/README.md). EKA results do not establish physical-device GPU behavior.
