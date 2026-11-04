# Hardware compositing

The browser page covers layer transforms, clipping, opacity, scrolling, resizing
and animation. Configure EKA2L1 as described in [Testing](../docs/TESTING.md).

```sh
python3 Tools/Symbian/tests/run-browser-graphics-tests.py /path/to/browser.exe \
  --suite compositing --output /tmp/compositing-results
```

The runner installs the fixture, launches the browser and collects its report,
logs and screenshot. A passing result completes the page's compositing checks.
Use [Composited text](compositing-text.md) to inspect text and stripe sharpness.
