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

- [x] HTML, CSS and Canvas 2D rendering with Skia CPU rasterization.
- [x] Ganesh GLES2 GPU rasterization for page tiles and Canvas 2D.
- [x] Shared BGRA textures, GPU Canvas copies and context resource lifetime management.
- [x] ARMv6 scalar raster pipeline with BGRA premultiplied surfaces.
- [x] System fonts, FreeType, HarfBuzz and WOFF2 web fonts.
- [x] Hardware composition through EGL and OpenGL ES 2.0.
- [x] Upstream Skia layer composition, image-set batching, masks, blend modes and filters.
- [x] Damage-aware composition with a persistent GPU frame.
- [x] WebGL 1 through ANGLE.
- [x] ScreenPlay presentation with mapped double buffers and dirty regions.
- [x] Bitmap presentation.
- [ ] Coordinated graphics and threaded tile painting.
- [ ] WebKit image atlases and tile DDL replay.
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

## Performance

- [x] Frame timing for rendering, uploads, ScreenPlay submission and image decoding.
- [x] Main-thread script, layout, paint and JIT compilation profiling.
- [x] Dirty-region tracking, incremental texture uploads and scrolling optimizations.
- [x] UI-priority image decoding and configurable JIT compiler priority.
- [ ] Measure Skia Canvas 2D and text rendering on Belle devices.
- [ ] Tune composited filter and mask surfaces.
- [ ] Add video frames as EGL textures.
- [ ] Add HTTP disk caching.
- [ ] Add A3F DevSound output for Web Audio.

Use `C:/Data/WebKit/frame-stats` to enable frame timing, open
`file:///C:/Data/WebKit/bench/index.html`, and summarize the browser log with
`tests/benchmarks/summarize-frame-stats.py`.

## Application packaging

- [x] Install the browser and shared data on C:.
- [ ] Split large executable images into native DLLs.

See [Version comparison](VersionComparison.md) for the 2.52.6 and 2.54.2 adaptation differences.
