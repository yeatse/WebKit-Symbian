# ANGLE over native EGL evidence

`final/result.json` records the actual 2026-10-29 EKA run: 34/34 checks, guest
exit 0, host exit 0, no reported panic/access violation. The unchanged guest log
identifies ANGLE's GLES renderer and checks shader compilation/linking, an exact
red framebuffer pixel, two virtual contexts, pthread context/error isolation,
rejection of ungenerated texture names/client arrays and normal teardown.
This is not a browser WebGL conformance result or evidence of native EGL shared
contexts. The tested native EGL implementation rejects nonzero share contexts;
ANGLE provides its own context virtualization.

The latest `bounded-read/result.json` records **45/45** on the same final host
as the depth test below. Image
`8148b8ff1b50acfc8bac5ef83b4ca37561742c3f163fc1dd27fff5c87ee19a33`
additionally checks the bounded GLES2 `GL_ReadPixelsRobustANGLE` API. A three-byte
RGBA buffer is rejected without writing its destination or guard bytes; a
four-byte buffer returns the exact red pixel, length 4 and dimensions 1×1 while
preserving its guards. Guest and host exits are zero, without a reported panic.
This tests ANGLE's memory validation in the firmware process. The adjacent
source hashes identify both the probe and the WebCore call-site snapshot, but
the standalone executable does not run WebCore. The separate
[browser result](../browser-final/README.md) subsequently passed 10/10
compositing and 9/9 WebGL checks on the same final host. Earlier 34/34 and 40/40
runs are preserved with their original images.

The later `depth-after/result.json` records **40/40**, with additional actual
depth storage, framebuffer completeness, precision, and accepted/rejected
fragment pixel checks. Its probe image is
`c7c2e67177bdee4bae99fbf9a99fc13b58e953859cddd2da0572e89e60b8b7fa`
and host is
`ff218e2e687c183c993aad2245824354e726f4c8bbefdff9b093b604602e18b1`.
Both processes exited zero. The original 34/34 remains a separate record.

`depth-before` is a real negative control: the old ANGLE format table advertised
`GL_OES_depth24` on GLES2 without checking the native extension, selected
`0x81A6`, and failed storage with `GL_INVALID_ENUM`. The corrected table requires
GLES3 or `GL_OES_depth24` for that renderbuffer format. On the tested GLES2 host,
ANGLE then selects the supported `0x81A5` (16-bit depth) without retrying a failed
allocation or disabling WebGL's depth buffer.

`depth-only-fbo-crash` records the next failure rather than treating successful
storage as a pass. ANGLE's robust initialization clears a depth renderbuffer via
a temporary FBO with no color attachment. EKA accepted that complete FBO but
dereferenced its missing color object in `ready_for_draw`. Patch `0016` supports
zero color attachments, consumes recycled FBO handles once, and implements real
null attachment removal for color/depth/stencil. Robust clearing remains enabled.
The same probe subsequently passed all 40 checks. The debugger log hash and
relevant frame names are retained in the manifest; the host-path-containing log
is not duplicated. `depth-after/patch-chain.json` records clean application of
all 13 ordered repository patches through `0016` to the fixed EKA base.

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
