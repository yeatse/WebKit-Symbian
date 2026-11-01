# C28 independent extra offline vectors — prepare-only delivery

This new external package leaves the original C28 fixture and every original FAIL/PASS artifact unchanged. No server, guest, display, audio, capture, or deployment was started during preparation. Its three public reports are documented in `fixture/README.md`: swapped merger pull order with exact 128-frame delay; mono Gain(1)→Delay128; independent Biquad lowpass/allpass standard magnitude/phase vectors. Python independently checks every A/B sample.

`run.py` derives from the fixed C28 wrapper `6b6561bb55135cff37461541c9ba3256568899ad60c8de92869142e44ec2deff`. It preserves dual runtime locks, no-active-host gate, host0032 pin, original whole-profile and six resource isolation, config/bindings/Qt/devices.yml preservation, normal own-PID parent-window close, host0 plus fresh fatal/nonzero-guest checks, no deadline/forced cleanup, all pins unchanged, and complete restoration. The cleanup block and preservation helpers retain baseline behavior. Preparation does not read private configuration contents; execution does not archive them.

The exact existing C28 image is fixed: 59961964 bytes, SHA-256 `ca1956441a681a3f9b7683d4f30654164a69507d1e8a6fc6d613c5a5059b3ad8`; its metadata is `97bcc390b937d7a763329d4a611d0504ed342cb36080f1c8df6396859490dddb`. Paths default under `/workspace/symbian-belle-work/candidate28`; overrides must still match those exact paths/hashes. The unchanged twelve candidate source snapshots and fourteen deployed native copies remain required. No new candidate metadata or runtime asset is generated or installed here.

Default mode only reads pinned public inputs and prints preparation; the proposed output is not created:

```sh
python3 -B /workspace/symbian-webaudio-offline-extra-c28-work/run.py \
  --output /workspace/webaudio-c28-offline-extra-input-check-01
```

Root's one execution command, when Root chooses to run it separately:

```sh
. /workspace/eka-validation/env.sh && . /workspace/symbian-media-work/env.sh && \
python3 -B /workspace/symbian-webaudio-offline-extra-c28-work/run.py \
  --execute --output /workspace/webaudio-c28-offline-extra-01
```

Every output must be fresh and canonical. The wrapper owns only its fresh-token finite localhost8866 fixture and browser parent: workflow deadline180s, server safety bound200s. The page runs automatically, with no click or capture. Exactly three reports and one fresh `WEBAUDIO-EXTRA-RESULT` precede a two-second health observation, own-PID close, fixture stop, original restoration, and final pin/public-state revalidation.

The page/validator emits `OFFLINE_EXTRA_API_COMPLETE`; only normal close, health, unchanged pins and full restoration permit wrapper `OFFLINE_EXTRA_RUN_COMPLETE`. These labels concern three offline numerical checks, not overall native runtime or PCM acceptance. JS numeric values alone do not identify an exact SDK helper symbol. `native_audio_verified` remains false; PCM/input-required flags are false for this offline-only workflow. Root's earlier scoped C28 native acceptance remains separate.

`fixture/test_guards.py` and `test_wrapper_gates.py` emit only `GUARDS_PASS_NOT_RUNTIME`. The delivery manifest freezes sources and public input identities; fixture files are separately frozen in `fixture/fixture-pins.json`. Unknown HTTP routes/origins/tokens, duplicate or unknown JSON fields, nonfinite values, repeated pages, stale/duplicate console records, and out-of-order/extra reports prevent completion.
