# Browser graphics

The runner opens the compositing and WebGL pages, checks their console results
and native presentation logs, and captures a screenshot. Configure EKA2L1 using
[Testing](../docs/TESTING.md).

```sh
python3 Tools/Symbian/tests/run-browser-graphics-tests.py /path/to/browser.exe \
  --suite all --output /tmp/graphics-results
```

Use `--suite compositing`, `--suite webgl` or `--suite screenplay` to select one
page. The default `auto` browser presents the ScreenPlay page through ScreenPlay
and switches to EGL for the compositing and WebGL pages.

Passing results include every page check, native surface presentation, browser
logs and a PNG screenshot. Review the screenshots using the
[compositing](compositing.md), [WebGL](webgl.md) and
[ScreenPlay](screenplay.html) fixtures. Each output directory contains
`result.json` and the selected suite's logs and screenshot.
