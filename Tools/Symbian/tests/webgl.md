# WebGL 1 browser checks

Open `file:///C:/Data/WebKit/webgl.html?autorun=1` in the Symbian browser.
The EKA browser runner copies all test HTML files into that directory.
Use a fresh browser log and a 90-second deadline. Require all nine group
markers, no `WEBGL FAIL`, normal guest exit, and the exact final marker:

```text
WEBGL DONE PASS 9/9 passed; 0 failed
```

The page checks real shader drawing and exact pixels, default and newly
allocated framebuffer clearing, precision and resource queries, invalid
inputs, resize, Canvas texture uploads with Y-flip and premultiplied alpha,
presentation into a 2D canvas, preservation across animation frames, and
context loss/restoration. The visible canvas should show opaque red above
half-transparent blue. Save a screenshot to check presentation in the page.

The same page passed all nine groups in host Chromium with SwiftShader.
That run checks the test's expectations only; it is not Symbian driver or
browser evidence. Symbian results must identify the browser and emulator
images and retain guest logs. This bounded sample is not a full WebGL
conformance run and does not test WebGL 2 or measure physical GPU performance.
