# Classic WebAudio fixture — prepared, not executed

This directory is external to the shared WebKit tree. Preparation and synthetic guard checks do not start a socket, guest, display, or audio device. No browser runtime or audible-output result has been obtained here.

The experimental build must enable classic WebAudio. The release default remains OFF. This fixture intentionally expects AudioWorklet's getter and constructors to be absent; it expects both media-element source entry points to throw `NotSupportedError` before the native context activates. It does not use microphone, MediaStream, media playback, injected input, private browser interfaces, or memory diagnostics.

## Prepare and inspect

Use a fresh absolute output directory on every invocation:

```sh
python3 fixture.py --output /workspace/wa-prepare-UNIQUE
python3 test_guards.py
```

The first command is prepare-only by default, creates a fresh `wa-<32 hex>` token, checks the frozen source hashes, and writes `manifest.json` and empty `state.json`. It never creates `ready.json` or a listener. The second command uses invented reports to exercise rejection guards and prints `GUARDS_PASS_NOT_RUNTIME`.

Root's preserved wrapper may explicitly launch the finite HTTP server:

```sh
python3 fixture.py --serve --run wa-FRESH32LOWERHEX --output /workspace/wa-runtime-UNIQUE/server --port 8864 --deadline 180
```

Replace the illustrative token with `wa-` followed by exactly 32 fresh lowercase hex digits. Read `ready.json`; navigate only to its `http://localhost:8864/?run=...` URL. Localhost is a potentially trustworthy origin and the page explicitly checks `isSecureContext`. The socket binds only to `127.0.0.1`; Host, Origin, token, route, payload size, strict JSON, event fields, sequence, and event types are checked. No unknown content is archived. Reloading the page, fetching the WAV twice, and malformed requests prevent completion. The fixture exits at its finite deadline, which cannot be accepted as successful completion; Root should stop it after owned host shutdown. `--serve` starts only the fixture, never the guest or audio.

## Public vectors

All offline contexts use sample rate 32768, modest buffers, and public browser APIs.

| Test | Independent expected result |
| --- | --- |
| Gain | `[1,-1,.5,-.5,.25,-.25,0,.125] × .5`, exact samples and zero tail in 128 frames |
| Delay/channel | 512 frames; input L frames 0=1 and 1=-.5, R frame 3=.25; swapped outputs, L delayed exactly 128 frames; exact samples and zero elsewhere |
| Oscillator | 1024 Hz sine, 4096 frames, first 256 skipped; frequency within 2 Hz, RMS within .01 of sqrt(.5), peak .98–1.01 |
| Analyser | Separate fixed sine buffer, 32 samples/period; FFT size 1024, smoothing 0; peak bin 32±1, above -30 dB, bins 16/48 at least 20 dB below peak |
| PCM16 WAV | Generated 60-byte mono WAV at the same rate; `[-32768,-16384,-1,0,1,8192,16384,32767] / 32768`, exact eight decoded samples |
| Unsupported decode | Fixed nine non-audio bytes reject as `EncodingError` |
| Unsupported APIs | Worklet getter and Window constructors absent; both media-element source entry points reject as `NotSupportedError`; native context remains suspended at time 0 |

These are behavioral vectors, not tests of private implementation algorithms. FFT normalization and sine phase are deliberately not reproduced or asserted exactly.

## Four real native button clicks

The page creates one native context without resuming it; after offline completion it exposes one button in a stable viewport. Root reads local `server/state.json`. The current public DOM button rectangle and viewport metrics are in `events[-1].layout` after each ready/interactive stage. Fields are CSS pixels: `innerWidth`, `innerHeight`, `scrollX`, `scrollY`, `button{x,y,width,height,disabled}`, optional `visualViewport{offsetLeft,offsetTop,width,height,scale}`, and the dimensionless `devicePixelRatio`. X11/guest chrome offsets and scaling must be calibrated by Root from the actual display; CSS coordinates are not XTest coordinates. Root records actual trusted XTest clicks and does not use JS input injection. The page ignores untrusted click events.

| Latest event | Enabled next click | Evidence produced |
| --- | --- | --- |
| `native-ready` | Start | `native-start`: trusted resume, 997 Hz sine, gain .15, state running |
| `native-running` | Suspend | Running currentTime advanced during a 1000 ms observation; suspend then holds time for 700 ms |
| `native-suspended` | Resume | Trusted resume, running time advances during 1000 ms observation |
| `native-resumed` | Close | Trusted close resolves to closed; button stays disabled; currentTime holds during 1000 ms bounded quiet |
| `quiet` | None | Console `WEBAUDIO-RESULT` status `PUBLIC_API_COMPLETE` |

Wait for the expected event before clicking again. The next handler is ready before an enabled-button layout is reported. All four click handlers call resume/suspend/close directly in the trusted event, before any unrelated asynchronous work. Public reports are still reports; they do not independently prove trusted host input, audible output, or silence.

```sh
python3 validate.py --state /workspace/wa-runtime-UNIQUE/server/state.json --run wa-FRESH32LOWERHEX --stage complete
```

The validator emits `PUBLIC_API_COMPLETE`, `OFFLINE_API_COMPLETE` (only the exact first nine reports with `--stage offline`), or `FAIL`. It **never emits whole-run PASS**, and always has `native_audio_verified: false`. Root must additionally prove a fresh native PCM capture has the expected 997 Hz spectral component while running, bounded quiet while suspended and closed, then combine normal owned-parent close, host/guest health, pin stability, and full profile/config/resource restoration. A JS running state or oscillator parameter alone is insufficient for a sound claim.

See [runner-plan.md](runner-plan.md) for the non-executing preserved-wrapper adaptation.
