# Composited text

`compositing-text.html` places matching plain and promoted text side by side,
including scrolled rows and one-pixel black-and-white stripes.

```sh
WAIT=60 Tools/Symbian/eka2l1/run-browser.sh /path/to/browser.exe \
  'file:///C:/Data/WebKit/compositing-text.html'
```

Capture the page after `READY: compositing-text`. Compare corresponding text and
stripe rows at the same viewport, font configuration and page scale. Matching
rows have the same geometry and sharpness.

`analyze-compositing-text.py --help` lists the screenshot coordinates and
`--require-match` option for checking the stripe rows.
