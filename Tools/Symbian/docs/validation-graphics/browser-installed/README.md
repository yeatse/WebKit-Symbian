# Installed browser graphics: 0.3.39

The browser installed from the three 0.3.39 SIS packages passed **compositing 10/10** and **WebGL 9/9** on 2026-10-29. The runner explicitly launched `E:\sys\bin\browser.exe`, after verifying all 11 installed package payloads by size and SHA256. Each page ran for its full 60-second observation period, logged native EGL initialization and a successful 360 × 532 presentation, and completed normal host shutdown with exit 0. No panic or compositor fallback was reported; the installed CA file was unchanged.

- Browser: `2db5f84970bfe15bdf107afc7c6a72f52a6acd6d3dce02493796173ea1d9e3cd`, 58,398,088 bytes.
- Browser SIS: `6f6c9749f81081d2337bdb3ae1ca867ec0eacb084eb6ea14333fab4531380e35`.
- Emulator: `ff218e2e687c183c993aad2245824354e726f4c8bbefdff9b093b604602e18b1`.
- Compositing source run: `browser-20261029T172805Z-lzoxznk9`.
- WebGL source run: `browser-20261029T172906Z-xweakl45`.

The [installation evidence](../../validation-release-0.3.39/README.md) records SIS controller/registry version 0.3.39, the exact installed payloads and subsequent identity checks. This run uses the existing strict graphics result validator. Its private launch driver replaces payload copying with identity checks and explicitly selects the E-drive executable; non-package HTML fixtures are still copied to C. `driver-adaptation.json` records that change and the original/adapted driver hashes.

`result.json`, both suites' logs and original 1280 × 900 screenshots are copied unchanged. `evidence.json` lists their hashes. `driver-source.sh.txt` and `runner-adapter.py.txt` preserve the actual private driver/adapter as audit text: they contain fixed paths from this validation workspace and are not portable launch scripts. The normal [runner instructions](../../../tests/browser-graphics-runner.md) describe supported invocation and locking.

The earlier [8ba0 browser result](../browser-final/README.md) and its [text/stripe pixel comparison](../compositing-text/README.md) remain separate historical evidence. This installed run repeats the two semantic graphics suites; it does not relabel the six-region pixel comparison as having been run on 2db5. Native presentation and page results are distinct checks. EKA results do not prove physical-device GPU behavior, a particular JIT tier, or complete WebGL conformance.
