# Project progress

Checked items are implemented. Unchecked items are ordered by priority within each module.

## Build and distribution

- [x] ARMv6/VFPv2 cross-build with the Symbian SDK.
- [x] E32 image conversion and one SIS package with the browser and ICU data; `jsc.exe` is distributed separately.
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
so emulator timings do not predict device performance. Turn on `frame-stats`
(in `C:\Data\WebKit\webkit.ini`), run `file:///C:/Data/WebKit/bench/index.html`
"Run all" with the screen kept on and nothing running in the background, and
summarize `log.txt` with `tests/benchmarks/summarize-frame-stats.py`.

### Done

- [x] Frame timing: EGL `render`, `flush`, `backing`, `draw`, `swap`;
  ScreenPlay `begin`, `paint`, `submit` (`copy`, `sync`, `attach`, `update`);
  `screenplay-paint`, `egl-draw`, `egl-surfaces`, `egl-backing` and
  `image-decode` lines.
- [x] Main-thread profile with frame-stats: `main-thread` (busy time per
  second by kind: script, style, layout, parse, paint, decode, gc, frame,
  other), `long-task` (each task of 100 ms or more, with its slowest script:
  eval URL, event type, timer, rAF or microtask) and `egl-render` /
  `egl-render-worst` (the `render` stage split by kind). Script is split
  further into `jsparse`, `bytecode` and `jit` (compiling on the main
  thread); `jit` lines count baseline and DFG compilations with how long
  plans waited for a compiler thread.
- [x] ICL decode latency: the ICL thread and the image decoding queues run at
  the UI thread's priority. On the 808 the whole decode fell from 2565 to
  326 ms/Mpx (251 in ICL, the rest waiting).
- [x] `backdrop-filter` is not drawn (`backdrop-filters = on` in
  `webkit.ini` turns it on): it drew everything under the element twice, about 10 ms a
  frame on YouTube.
- [x] Benchmark pages: scroll, images, animation, composited, canvas, webgl,
  and paint (ten full-repaint cases plus `paint-images`).
- [x] GPU tiles for every page (default): the page is composited into
  full-width, 256-pixel-high tiles held in textures, capped at 32 MB
  (`gpu-tiles = off` falls back to ScreenPlay). Tiles are prefetched one screen
  ahead, at most two per frame across all layers, and skipped after a slow
  visible repaint. The damage in one update is painted once and uploaded
  straight from the paint buffer (the 808 has no `EXT_unpack_subimage`).
- [x] ScreenPlay fallback: cached shadow frame buffer (surface memory reads
  cost ~130 ns per pixel), scroll blits, up to four damage rectangles, and the
  surface kept when the on-screen keyboard shrinks the view.
- [x] Painting: bilinear image scaling, pixman ARMv6 SIMD, an A8 text-shadow
  path with a per-glyph blurred shadow cache, the box-shadow template cache
  with nearest-neighbour edge stretching, and a 4 MB cache of decoded images
  scaled to their drawn size.
- [x] Composited video verified on the 808: 87 s of playback in the #110 log.
- [x] JPEG through the ICL decoder on its own thread for complete YCbCr or
  grey images up to 2.5 Mpx; libjpeg otherwise. The 808 has only the Nokia
  software plug-in 0x101FF555, but it decodes 2.5-3.4x faster than our
  libjpeg in the probe.
- [x] Compositor robustness: shader link failures fall back to simpler
  variants, the rounded-clip loop is unrolled for VideoCore IV, GL errors are
  traced per step, and only three failed frames in a row fall back to software.
- [x] Fewer intermediate surfaces: inert filters are dropped, `overflow:hidden`
  containers that paint nothing do not force overlap, and `inset()`,
  `circle()`, `ellipse()` and box clip-paths use the rounded-clip shader.
- [x] Video under the page: MMF plays in a window below the browser window and
  the composited page punches a transparent hole, so EGL composition keeps
  running during playback (`composited-video = off` restores the old path).
- [x] Volume keys for media and Web Audio, ten steps, default 4.

Latest 808 numbers (CI #87 and #100):

| Case | fps | Note |
| --- | --- | --- |
| scroll | 55 | GPU tiles; ScreenPlay was 26 |
| images | 60 | ScreenPlay was 26 |
| animation | 24 | relayout every frame |
| composited | 33 | |
| canvas | 7.5 | ~105 ms of Cairo drawing per frame |
| webgl | 17.5 | ~39 ms of JS and GL calls per frame |
| paint-text / text-shadow / shadow / rounded | 21 / 15 / 15 / 15 | full repaint |
| paint-images | 27 | |
| YouTube results and watch page | ~5 | render and backing spikes, below |

### Open, by priority

- [ ] P0: Main thread taken by unlabelled work before a YouTube video
  starts: in the #111 logs "other" is all of the main thread for 40-90 s, in
  30-50 tasks a second of about 25 ms, with one frame in five seconds. The
  profile now splits it into `net`, `media` (writing the download to its
  file) and `timers`: collect a log and move the culprit off the thread.
- [ ] P0: YouTube script time: 81 s running, 9 s parsing, 5 s generating
  bytecode in the #111 logs, so a bytecode cache would save little. JIT plans
  wait 2.5 s on average for a compiler thread (21 s at most);
  `jit-priority = on` ends the wait but the compiler then takes
  the main thread's time, with no clear gain. Next: `bench/jsbench.html` with
  `js-tier` llint, baseline and dfg, to see what each tier is worth on
  the 808, and whether holding timers during input (now the default,
  `timer-deferral = off` turns it off) makes scrolling smoother.
- [ ] P0: YouTube compositor time, 32% of busy time (draw 21.8 ms and backing
  12.9 ms a frame). Check draw without backdrop filters, then the remaining
  filter and mask surfaces.
- [ ] P1: Canvas 2D (~105 ms per frame in Cairo) and WebGL (~39 ms per frame):
  profile which calls dominate before choosing a fix.
- [ ] P1: Remaining intermediate surfaces on YouTube (up to 33 binds per
  frame: about 6 filter-mask surfaces and a backdrop); the clip-path shader
  path did not apply there (`shape-clips=0`).
- [ ] P1: HTTP disk cache, the largest remaining load-time cost.
- [ ] P1: Video frames as GL textures (EGL_NOK_image_endpoint2) for video in
  opacity, rounded or transformed layers, Canvas 2D and WebGL; advertise
  H.264 main profile if the 808 decoder accepts it.
- [ ] P2: Media Source Extensions over DevVideo or OpenMAX IL, after
  confirming that third-party applications can use them on the device.
- [ ] P2: A3F DevSound output for lower Web Audio latency.
- [ ] Before WebKit 2.54: compare Cairo, Skia CPU (ARM11 has no NEON) and Skia
  GPU on the benchmark pages; 2.54 removes Cairo.

The 808 has OpenVG and `EGL_KHR_vg_parent_image`, but WebKit has no OpenVG
backend and 2.54 drops Cairo, so an OpenVG painter is not planned. OpenWF-C is
a device-manufacturer interface and the 808 has one core.

## Deferred

- [ ] Split browser.exe into DLLs so the package can install on E:. The split
  images panicked USER 224 on Belle FP1 and the work was reverted; the SIS
  installs on C: until this is picked up again.
