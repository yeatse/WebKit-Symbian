# Browser compositing and WebGL regression

The strict browser runner passed both suites on 2026-10-29: **compositing 10/10** and **WebGL 9/9**. Each run separately logged native EGL/GLES2 initialization and a successful 360 × 532 presentation, with no compositor fallback or panic. Both browser drivers returned 0 after the observation period and graceful host shutdown. The installed CA file was unchanged.

- Browser SHA256: `8ba0a5068a3dbd3aef616d71ae3b9f6d6b0a906c376c49fa4a9cc04685eaead5`.
- Emulator SHA256: `ff218e2e687c183c993aad2245824354e726f4c8bbefdff9b093b604602e18b1`.
- Original runner output: `/workspace/symbian-graphics-work/graphics-final-8ba0-ff218`.
- Compositing source run: `browser-20261029T155851Z-aifuo9s4`.
- WebGL source run: `browser-20261029T155952Z-_dkoclzi`.

`result.json` is the unmodified runner result, including run-specific page URLs, exact terminal markers, image dimensions, presentation dimensions, exit status, and CA hashes. Its absolute paths identify the original run; portable copies of every referenced log and screenshot are in the `compositing/` and `webgl/` directories here. `evidence.json` records hashes for these copies. Screenshots are original 1280 × 900 captures, without resizing or other edits.

The compositing page checks transformed geometry, opacity, overlapping hit testing, overflow clipping, layer removal, ordinary root repaint, scrolling, resize, animation, and frame delivery. Geometry checks alone had previously passed despite blurred promoted text; the same repaired browser also passed the separate [text and one-pixel stripe comparison](../compositing-text/README.md).

The WebGL page checks context/capabilities, cleared new storage, shader/texture/vertex queries, framebuffer allocation, validation, resize, canvas upload orientation and alpha handling, canvas presentation/preservation, and context loss/restoration. Its deliberately invalid texture target and out-of-range buffer update produce expected WebGL error messages before the validation check passes; those are negative test inputs. This is WebGL 1 coverage, not a WebGL 2 or full conformance result.

The runner requires page results and native presentation independently. These EKA results demonstrate the browser's native EGL path and ANGLE WebGL behavior under the emulator; they do not establish a physical-device GPU result or prove a particular JavaScript tier. The GUI browser is observed and shut down by the host driver; there is no invented standalone guest `main()` exit marker.

To repeat with the configured emulator, SDK data, and display environment:

```sh
python3 Tools/Symbian/tests/run-browser-graphics-tests.py /path/to/browser.exe \
    --suite all --timeout 60 --data /path/to/eka-data --output /new/result/directory \
    --browser-sha256 8ba0a5068a3dbd3aef616d71ae3b9f6d6b0a906c376c49fa4a9cc04685eaead5 \
    --emulator-sha256 ff218e2e687c183c993aad2245824354e726f4c8bbefdff9b093b604602e18b1
```

See [the runner instructions](../../../tests/browser-graphics-runner.md) for environment variables and locking requirements. The captured `Fontconfig` missing-default-config warning remains in the raw logs; these pages nevertheless rendered their text and passed the recorded checks.
