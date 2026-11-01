# Classic WebAudio PCM tools — prepared, not executed

These external tools have not opened Pulse, guest, display or a server. Tests
use invented arrays and JSON. Capture defaults to preparation; actual recording
requires Root's explicit `--execute` and existing private media environment.

The fixed source is `symbian_media.monitor` at
`unix:/workspace/symbian-media-work/runtime/native`, the isolated test null sink.
No default source, microphone, other server or user sound device is used.
The format is 48000 Hz stereo signed-16 little-endian, for at most 180 seconds.

## Root commands

Choose a fresh output directory every time. This command only prepares metadata:

```sh
python3 capture.py --output /workspace/wa-pcm-PREPARE-UNIQUE
```

When the experimental image is ready, start the actual command as an owned
managed process before the first native Start click:

```sh
. /workspace/symbian-media-work/env.sh
python3 capture.py --execute --max-seconds 120 --output /workspace/wa-pcm-RUN-UNIQUE
```

Wait for `ready.json` (`PCM_BYTES_RECEIVED`). Record eight host observations with
the fresh fixture token and exact labels below. For `click-*`, invoke immediately
after the actual XTest action, retaining its independent input record. Other
labels mean Root has just observed that public fixture event.

```sh
python3 mark.py --output /workspace/wa-pcm-RUN-UNIQUE --run wa-FRESH32HEX --label click-start
python3 mark.py --output /workspace/wa-pcm-RUN-UNIQUE --run wa-FRESH32HEX --label native-running
# Hold at least 1.2 seconds before Suspend.
python3 mark.py --output /workspace/wa-pcm-RUN-UNIQUE --run wa-FRESH32HEX --label click-suspend
python3 mark.py --output /workspace/wa-pcm-RUN-UNIQUE --run wa-FRESH32HEX --label native-suspended
# Hold at least 1.2 seconds before Resume.
python3 mark.py --output /workspace/wa-pcm-RUN-UNIQUE --run wa-FRESH32HEX --label click-resume
python3 mark.py --output /workspace/wa-pcm-RUN-UNIQUE --run wa-FRESH32HEX --label native-resumed
# Hold at least 1.2 seconds before Close.
python3 mark.py --output /workspace/wa-pcm-RUN-UNIQUE --run wa-FRESH32HEX --label click-close
python3 mark.py --output /workspace/wa-pcm-RUN-UNIQUE --run wa-FRESH32HEX --label quiet
# Hold at least 1.2 seconds before stopping capture.
```

Root can instead write the identical `marks.json` contract from actual input
timestamps and fixture `received_monotonic[]`, matched to `events[]` by index.
The outer wrapper exports these as `public_event_times[].server_received_monotonic`.
Use the same host monotonic clock, not guessed guest times. Labels must remain
in exact order; these records alone cannot prove trusted input.

Then send SIGTERM or SIGINT to **only the owned capture Python process**, using
its managed handle/PID. It sends SIGINT only to its own pacat child and waits
at most three seconds. Forced kill, early EOF/exit, incomplete writes or deadline
cannot satisfy capture acceptance. Its metadata retains child PID and exit,
stop signals, cleanup, full PCM/hash, first/last byte observations and periodic
timing anchors. Pacat 0 or SIGINT (-2) is allowed after the requested stop.

```sh
python3 analyze.py --output /workspace/wa-pcm-RUN-UNIQUE
```

## Predeclared observations

Each public stage marker selects host marker +300..800 ms. The stage must hold
at least 1.2 s before the next click; final quiet must hold at least 1.2 s before
capture stop. The analyzer takes the union of sample ranges from **every**
byte-count/delivery-time alignment estimate. Estimate spread over 150 ms rejects
analysis. Spread measures delivery variation, not unknown constant queue delay:
these estimates are not hardware timestamps or true bounds on synchronization.
The report explicitly keeps `timing_alignment_verified: false`.

Both running stages require both-channel RMS >100 in signed-16 units, Hann FFT
peak at 997±3 Hz, at least 95% power in 997±10 Hz, peak below 32760, and no
continuous exact-zero gap >=1 ms in either channel. Windows contain at least
500 ms, so FFT bins are at most 2 Hz. Suspended and closed require both-channel
RMS <=2; `exact_zero` separately reports whether all samples are exactly zero.
Low nonzero quiet is not described as exact silence. Gain .15 is not used to
predict exact native amplitude. A 10 ms stereo-RMS activity timeline also
reports actual captured tone/quiet geometry independently of marker alignment.

Success is only `PCM_OBSERVATIONS_COMPLETE`; `whole_run_verified` always stays
false. Root must separately combine public API completion, actual trusted input,
normal owned-browser close, no new fatal outcome, frozen image/assets and full
runtime/profile/config restoration. Tone samples alone do not complete WebAudio.
No exact guest latency, seamless pause or hardware timestamp claim is made.

## Checks performed

```sh
python3 test_guards.py
```

Six test methods exercise matching invented tones/quiet and reject wrong source,
unexecuted preparation, child/deadline/cleanup failures, bad hashes/frame alignment,
missing/reordered markers, timing uncertainty, short holds, wrong frequency,
weak/missing channels, clipping, single-channel zero gaps, mixed tones, nonfinite
data and nonquiet windows. They do not record or run anything.

`prepare-only/` contains only real-tool preparation metadata, not PCM or a
capture result. `validation.json` distinguishes synthetic checks and prepare-only
inspection from the still-unrun actual recording.
