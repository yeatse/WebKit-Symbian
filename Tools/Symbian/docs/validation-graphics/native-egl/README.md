# Native EGL evidence

Recorded on 2026-10-29 with the X7-00 (RM-707) Belle firmware in EKA2L1.
The latest `final-clean/result.json` records **131/131**, nine exact pixel
checks and three swaps on host
`ff218e2e687c183c993aad2245824354e726f4c8bbefdff9b093b604602e18b1`,
without temporary `MMFSTART` audio diagnostics. It uses the unchanged probe
`bd8fce0b249e73edbe0eb9661b1d8d6a39b9adafa0b2340981e5ae1a5a1c9aa3`,
with effective hold 15,000 ms and ES3 rejection enabled; guest and host exits
are zero with no reported panic/access violation. Original guest log, result
and resized-window screenshot are preserved in that directory.

The historical `final/result.json` is the unmodified runner record: 123/123, nine exact pixel
checks, three swaps, effective hold 15,000 ms, ES3 rejection requested and
observed, guest exit 0 and host exit 0. `final/screenshot-1.png` shows the actual
208×272 resized purple window. This does not demonstrate physical-device GPU
execution or browser canvas presentation.
The separate [final browser result](../browser-final/README.md) records its own
10/10 compositing and 9/9 WebGL checks on the same final host.

The later `viewport-query/result.json` adds eight current-state checks and
passes **131/131**, retaining all nine pixels and three swaps. It queries the
four exact values after setting a nonzero viewport origin, restores the prior
viewport, and checks scissor, enable, program and framebuffer state used by
TextureMapper. Patch `0014` supplies the previously missing `GL_VIEWPORT`
getter from the original guest coordinates, before driver scaling/Y inversion.
The image is `bd8fce0b249e73edbe0eb9661b1d8d6a39b9adafa0b2340981e5ae1a5a1c9aa3`;
the host is `e2a76526c006863894403872343abd5d1f051bce647c67d4f22b68dbe02d5704`.
That host also contains temporary unrelated audio diagnostics. Its exact image,
patch assets and runner result are recorded separately; it does not replace the
earlier 123/123 record or change the host identity of the ANGLE 34/34 run.
The added probe/patch hashes are in `viewport-query/source-hashes.json`.

`manifest.json` maps every retained file to its original run, byte length and
SHA-256. The `.log.gz` files decompress to the exact original guest log bytes;
result JSON and PNG files are copied unchanged. `precision-null`, `object-name`
and `default-framebuffer` preserve the three failing stages, including the
host SIGSEGV and subsequent guest failures. A null guest exit in an early result
record is left unchanged rather than inferred from later runs. See
[GRAPHICS.md](../../GRAPHICS.md) for the diagnosed causes and fixes.

`precision-loader.log` is the unmodified output from the host regression:

```sh
bash Tools/Symbian/tests/egl-probe/check-precision-loader.sh "$EKA_SOURCE"
```

It compiles the actual patched GLAD loader against controlled resolver/capability
fixtures and checks seven cases. Its values are forwarding fixtures, not claimed
hardware precision. The real guest log supplies the separate hardware-API query
evidence. `patch-chain.json` records clean sequential application of the ten
listed patches to EKA base `5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a`.
`source-hashes.json` identifies the probes, runner and patches used for this
snapshot; later repository changes need fresh validation.

Build using [the probe instructions](../../../tests/egl-probe/README.md).
With the isolated firmware environment already selected, run:

```sh
python3 Tools/Symbian/eka2l1/run-native-probe.py \
    --probe egl --image "$EGL_PROBE" --image-sha256 "$EGL_PROBE_SHA256" \
    --emulator "$EKA2L1" --emulator-sha256 "$EKA2L1_SHA256" \
    --reports "$REPORTS" --timeout 60 \
    --screenshot-marker 'READY: egl-window-probe' \
    -- --expect-es3-rejection --hold-ms 15000
```

Only request ES3 rejection for the tested EKA ES2 implementation, not arbitrary
physical devices. The full host logs contain machine paths and are not copied;
their hashes remain in the manifest and their strict exit/panic results remain
in the original JSON. No pass is inferred merely from a screenshot or GL string.
