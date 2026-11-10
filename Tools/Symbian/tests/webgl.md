# WebGL

The browser page checks shader compilation, buffers, textures, framebuffer
rendering and context lifecycle. Configure EKA2L1 using
[Testing](../docs/TESTING.md).

```sh
python3 Tools/Symbian/tests/run-browser-graphics-tests.py /path/to/browser.exe \
  --suite webgl --output /tmp/webgl-results
```

The runner installs the fixture and collects the page report, browser logs and
screenshot. A passing result completes the WebGL checks with the expected pixels.
