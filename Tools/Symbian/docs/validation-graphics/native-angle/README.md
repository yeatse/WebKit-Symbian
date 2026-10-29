# ANGLE over native EGL evidence

`final/result.json` records the actual 2026-10-29 EKA run: 34/34 checks, guest
exit 0, host exit 0, no reported panic/access violation. The unchanged guest log
identifies ANGLE's GLES renderer and checks shader compilation/linking, an exact
red framebuffer pixel, two virtual contexts, pthread context/error isolation,
rejection of ungenerated texture names/client arrays and normal teardown.
This is not a browser WebGL conformance result or evidence of native EGL shared
contexts. The tested native EGL implementation rejects nonzero share contexts;
ANGLE provides its own context virtualization.

`missing-client-extension` preserves the initial `EGL_NO_DISPLAY` failure.
Symbian was missing the client-extension declaration needed to validate its
implemented EGL device platform attribute. The final `Display.cpp` declaration
fix allowed the real backend to initialize; no fake renderer or native API alias
was substituted. See [GRAPHICS.md](../../GRAPHICS.md).

`manifest.json` records exact original run IDs and uncompressed log hashes.
Guest logs are losslessly gzip-compressed; result JSON is unchanged. Host logs
with machine paths are not duplicated, but their hashes and original exit/panic
summaries are retained. `artifacts.json` records static-library/image hashes and
the `llvm-nm` check: zero public `egl*`/`gl*` aliases, 963 private `EGL_*`/`GL_*`
entry points and zero unsupported ELF TLS references. Probe and patch source
hashes are shared with `../native-egl/source-hashes.json`.

Build using [the probe instructions](../../../tests/angle-probe/README.md), then
with the isolated firmware environment already selected:

```sh
python3 Tools/Symbian/eka2l1/run-native-probe.py \
    --probe angle --image "$ANGLE_PROBE" --image-sha256 "$ANGLE_PROBE_SHA256" \
    --emulator "$EKA2L1" --emulator-sha256 "$EKA2L1_SHA256" \
    --reports "$REPORTS" --timeout 60
```

No guest arguments are required. The finite runner must complete its normal
close and log collection; timeout/forced termination remains a failure.
