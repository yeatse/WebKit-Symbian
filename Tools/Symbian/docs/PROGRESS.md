# Project progress

Checked items are implemented. Unchecked items are ordered by priority within each module.

## Build and distribution

- [x] ARMv6/VFPv2 cross-build with the Symbian SDK.
- [x] E32 image conversion and one SIS package with the browser, JavaScriptCore, ICU data and example scripts.
- [x] Automatic GitHub Actions builds, release tags and certificate signing.
- [x] Reproducible SDK interfaces, runtime libraries and build tools.
- [ ] Expand physical-device compatibility and performance coverage.

## JavaScript on ARM11

- [x] ARM LLInt, Baseline JIT and DFG with on-stack replacement.
- [x] Concurrent DFG compilation.
- [x] ICU 70.1 and JavaScript internationalization.
- [x] Typed arrays, resizable buffers and garbage collection.
- [x] Dedicated Web Workers.
- [ ] Shared Workers.

## Rendering and graphics

- [x] HTML, CSS and Canvas 2D rendering with Cairo.
- [x] System fonts, FreeType, HarfBuzz and WOFF2 web fonts.
- [x] Hardware composition through EGL and OpenGL ES 2.0.
- [x] WebGL 1 through ANGLE.
- [x] ScreenPlay presentation with mapped double buffers and dirty regions.
- [x] Bitmap presentation.
- [ ] Multitouch and pinch gestures.
- [ ] Fullscreen API.

## Browser and input

- [x] AVKON browser application with URL entry, menus, bookmarks and history.
- [x] Native form, selection, prompt and authentication dialogs.
- [x] Touch and pointer input, FEP text editing and plain-text system clipboard.
- [x] Single-file uploads and downloads with save and cancel controls.
- [ ] Multiple-file uploads.
- [ ] Rich-text clipboard and Async Clipboard API.
- [ ] System sharing and notifications.

## Networking and security

- [x] HTTP, HTTPS, WebSocket and secure WebSocket.
- [x] Certificate chain, hostname and IP address verification.
- [x] IPv6 and HTTP/2.
- [x] HTTP Basic and Digest authentication.
- [x] Persistent cookies with SameSite support.
- [x] WebCrypto AES-CBC, AES-CTR, AES-GCM, AES-KW, HMAC, HKDF and PBKDF2.
- [x] WebCrypto RSA, ECDH, ECDSA and SHA algorithms.
- [ ] IAP proxy configuration, proxy authentication and DNS routing.
- [ ] HTTP disk cache.
- [ ] System certificate store and client certificate selection.
- [ ] Ed25519 and X25519.
- [ ] Protected CryptoKey persistence and structured cloning.

## Storage and background work

- [x] localStorage and sessionStorage.
- [x] IndexedDB with persistent Blob storage and quota handling.
- [x] BroadcastChannel.
- [ ] Cookie Store API.
- [ ] Cache API and StorageManager.
- [ ] Service Workers.

## Media and audio

- [x] HTML audio and video playback through Symbian MMF, including URL sources.
- [x] Playback controls, seeking and volume.
- [x] Web Audio with MMF PCM output and WAV decoding.
- [x] Core audio nodes, FFT processing and HRTF spatial audio.
- [ ] Video frames in Canvas 2D and WebGL textures.
- [ ] Media Session API.
- [ ] AudioWorklet and media-element PCM sources.
- [ ] Media Source Extensions and streaming-media integration.

## System integration

- [x] Chunk-backed heap growth and ARM runtime allocation support.
- [x] Native monotonic timing and connection and memory observers.
- [ ] Complete memory-pressure and online/offline lifecycle handling.
- [ ] Geolocation.
- [ ] Device orientation and motion sensors.
- [ ] Vibration API.

## Symbian^3 graphics and media performance

Measure each step on a Nokia 808 PureView: EKA2L1 maps GLES2 onto the host GPU,
so emulator timings do not predict device performance.

- [x] P0: Frame timing in the browser log. `frame-stats` (a launch option or
  `C:\Data\WebKit\frame-stats`) logs average/maximum milliseconds per stage
  about once a second: EGL `render` (animation frame callbacks, style and
  layout), `flush` (layer preparation), `backing` (painting and texture
  upload), `draw` and `swap` (`render` and `flush` were one `update` stage
  until 5a1f5d56843); ScreenPlay `begin`, `paint` and `submit`, with
  `screenplay-submit` splitting the submit into `copy`, `sync`, `attach` and
  `update`; bitmap `paint` and `blit`.
- [x] P0: Fixed scrolling, image, animation and WebGL pages, measured with
  `frame-stats` on the 808 for ScreenPlay, EGL and bitmap presentation.
  The pages and a log summarizer are in `tests/benchmarks` and installed at
  `C:\Data\WebKit\bench`. First 808 run (fps, then ms per frame by stage):

  | Page | Path | fps | Stages (average ms) |
  | --- | --- | --- | --- |
  | scroll | ScreenPlay | 17.9 | begin 1.5, paint 17.5, submit 26.7 |
  | images | ScreenPlay | 5.9 | begin 1.5, paint 130.9, submit 25.3 |
  | animation | ScreenPlay | 17.2 | begin 1.3, paint 14.3, submit 25.5 |
  | composited | EGL | 26.6 | update 1.2, backing 0.2, draw 6.1, swap 3.9 |
  | canvas | EGL | 7.6 | update 104.9, backing 20.2, draw 3.6, swap 0.7 |
  | webgl | EGL | 17.8 | update 40.0, backing 0.2, draw 10.5, swap 0.9 |

  ScreenPlay's submit costs about 26 ms on every frame, whatever was
  painted; scaled images (images) and 2D canvas drawing (canvas, presented
  through EGL) are the slow paints; WebGL spends 40 ms per frame in update.

  Third run (6d2b9309271): bilinear image scaling and painting ScreenPlay
  frames in cached memory (reading surface memory costs ~130 ns per pixel,
  so flattening in place took 25 ms) gave scroll 25.9 fps (submit 11.3 ms,
  of which copying into the surface 9.5), images 15.0 (paint 44.5),
  animation 22.6 and composited 29.7; canvas 7.5 and webgl 17.7 unchanged.
- [x] P0: The device's EGL and GLES2 extensions (EGLImage, preserved swap and
  partial update), before work that depends on them. The 808 (VideoCore IV,
  EGL 1.4, GLES 2.0) has EGL_KHR_image_base, EGL_KHR_gl_texture_2D_image,
  EGL_KHR_lock_surface2 and GL_OES_EGL_image, but destroyed swaps and no
  partial update or buffer age extension.
- [x] P0: ScreenPlay for 2D pages and EGL composition on demand. The default
  `auto` build creates the compositor when WebCore attaches composited layers,
  after ScreenPlay detaches, and returns to ScreenPlay two seconds after the
  layers are removed. Verified in EKA2L1 by the compositing, WebGL and
  ScreenPlay suites: ScreenPlay, then EGL (started in about 16 ms), then
  ScreenPlay again on the same window.
- [ ] P1: Partial texture uploads and a tiled root layer, preserved swaps with
  scissored composition, and a tile margin beyond the viewport for scrolling.
- [ ] P1: Hardware JPEG decoding through ICL `CImageDecoder`, scaled to the
  displayed size.
- [ ] P1: Video surfaces that follow layer scrolling and transforms; EGLImage
  import of MMF surfaces for Canvas 2D and WebGL video frames.
- [ ] P1: HTTP disk cache, the largest remaining load-time cost.
- [ ] P2: Media Source Extensions over DevVideo or OpenMAX IL, after
  confirming that third-party applications can use them on the device.
- [ ] P2: A3F DevSound output for lower Web Audio latency.
- [ ] Before WebKit 2.54: compare Cairo, Skia CPU (ARM11 has no NEON) and Skia
  GPU on the P0 pages; 2.54 removes Cairo.

OpenVG has no WebKit backend, OpenWF-C is a device-manufacturer interface and the
808 has one core, so these are not planned.
