# C28 classic WebAudio preserved wrapper — prepared, not executed

External derivative of the pinned preserved runner `/workspace/symbian-multi-upload-work/runtime-c25-v2-host0032/run.py` (SHA-256 `1b2177f47770b075b3d140c49ff2288d94353e8387e2befccdf3140f07a2c2a7`). No guest/server/display/audio was started, nothing was deployed, and shared source/runtime files were not modified during preparation.

Default preparation only reads public artifact identities and prints JSON. It creates no proposed output directory, acquires no runtime lock, reads no private profile/configuration contents, and starts no subprocess:

```sh
python3 -B /workspace/symbian-webaudio-classic-runtime-c28-host0032-work/run.py \
  --output /workspace/webaudio-c28-input-check-01
```

`prepare-only-contract.json` records an actual preparation. All fourteen native copies now match their fixed pins. The remaining blockers are the missing actual C28 image/hash and C28 build/source metadata. The wrapper never deploys native patches.

The two templates intentionally have null image/source hashes. Missing/partial CLI pins, invalid SHA-256, missing/symlinked/noncanonical files, actual SHA mismatch, C25 identity, wrong feature/source metadata, or any wrong native copy prevent execution. `ready_to_execute` covers input identities; actual environment/host/lock ownership is checked at execution.

After the actual C28 build succeeds, Root supplies its reviewed frozen image SHA-256 and runs the metadata-only helper. It requires actual candidate28 build-identities.json, source-identities.json, and twelve source snapshots under source/. It checks snapshot bytes against the actual shared build source, freezes actual cache/config/generated Settings hashes, and verifies WebAudio ON. It does not launch or deploy:

```sh
C28_WEB_AUDIO_EXE=/workspace/symbian-belle-work/candidate28/browser.exe
C28_WEB_AUDIO_SHA256=REPLACE_WITH_ACTUAL_FROZEN_64_LOWERCASE_HEX_DIGEST
python3 -B /workspace/symbian-webaudio-classic-runtime-c28-host0032-work/freeze_metadata.py \
  --browser "$C28_WEB_AUDIO_EXE" --browser-sha256 "$C28_WEB_AUDIO_SHA256" \
  --output /workspace/symbian-belle-work/candidate28/runtime-metadata.json
# Root copies the helper's reported metadata SHA-256 into this variable.
C28_WEB_AUDIO_METADATA_SHA256=REPLACE_WITH_ACTUAL_FROZEN_METADATA_DIGEST
python3 -B /workspace/symbian-webaudio-classic-runtime-c28-host0032-work/run.py \
  --browser "$C28_WEB_AUDIO_EXE" --browser-sha256 "$C28_WEB_AUDIO_SHA256" \
  --candidate-metadata /workspace/symbian-belle-work/candidate28/runtime-metadata.json \
  --candidate-metadata-sha256 "$C28_WEB_AUDIO_METADATA_SHA256" \
  --output /workspace/webaudio-c28-input-check-02
```

Metadata binds candidate_id=C28 and the actual supplied image path/hash. It requires actual `/workspace/symbian-graphics-work/build/webcore/CMakeCache.txt` WebAudio ON and correct source root, actual cmakeconfig.h WebAudio=1, and generated Settings.cpp's OS(SYMBIAN) worklet-default-false branch. There is **no invented AudioWorklet compile flag**: worklet code may compile; runtime settings/interfaces are disabled and public absence is separately tested. The three unchanged UI resources default to the image directory; optional absolute `--resource-dir` selects final packaging without changing their fixed resource hashes.

Only after all preparation blockers are cleared, Root executes:

```sh
. /workspace/eka-validation/env.sh
. /workspace/symbian-media-work/env.sh
python3 -B /workspace/symbian-webaudio-classic-runtime-c28-host0032-work/run.py \
  --execute --browser "$C28_WEB_AUDIO_EXE" --browser-sha256 "$C28_WEB_AUDIO_SHA256" \
  --candidate-metadata /workspace/symbian-belle-work/candidate28/runtime-metadata.json \
  --candidate-metadata-sha256 "$C28_WEB_AUDIO_METADATA_SHA256" \
  --output /workspace/webaudio-c28-native-01
```

Every output must be fresh, canonical and absolute. Root alone controls actual XTest clicks and bounded Pulse capture; this wrapper has no input/screenshot/capture command. Start capture before Start. Read host-pid.public and server/ready.json. The owned finite server is localhost8864 with a fresh wa-token; host workflow deadline is 180s, server safety deadline 200s. The wrapper preserves a two-second health/quiet observation after complete public API reports.

Wait native-ready→Start, native-running→Suspend, native-suspended→Resume, native-resumed→Close, quiet→done. Root holds every observed native-running/suspended/resumed/quiet stage at least 1.2s. Server received_monotonic aligns with events and uses host monotonic time; final result.public_event_times provides kind/sequence/server-receive-time, plus host launch/public-completion/close-request times. Root separately records actual click times. PCM analysis uses marker+.3..+.8s without claiming exact hardware synchronization. Public DOM rectangles/viewport metrics and vectors are documented in `/workspace/symbian-webaudio-classic-fixture-work/README.md`.

Preserved gates: fixed host0032 `692abe6937ed1b651914ff6ab35a78bbd3693a705ba47853fc6f9a438ae178bd`; pinned fixture/helpers/resources; fourteen native copies (original ten MMF/FEP plus actual ostream DLL/map in both runtime/build patch directories); dual locks; no active host; disabled cores; isolated XDG/native Pulse; whole original WebKit profile and six resource originals; full config.yml/bindings/Qt/devices.yml clone and restore; fresh fatal/nonzero-guest checks; exact fresh browser completion; own-PID normal parent close; exit0; no deadline/forced cleanup; all pins unchanged; complete restoration; final public revalidation. A remaining active host prevents restoration and retains private originals/journals. Private profile/configuration contents are never archived.

The known `DATA/devices.yml` file is included as a file in the same PrivateConfigurations transaction because host device loading rewrites it on each launch. Preparation does not open or print its contents. A pure source guard checks that this required preservation specification remains present; runtime restoration is still governed by the pinned transaction helper and whole-configuration completion gate.

The actual ostream DLL source is `artifacts/mediaclientaudiostream_general.dll`, 40384B SHA-256 `8559122f1caeea69e9612ee68f641f9ca90501a12183c5fbd3b77368cdbede04`; genuine ordinal map is `artifacts/mediaclientaudiostream.dll.map`, 5027B SHA-256 `e92b9ede1b1a11355120e7a661be025f795e86841deec5c41b78fe7b691bc4ce`, under `/workspace/symbian-ostream-pause-native-build-20261101`. No `.elf.map` is accepted or copied.

Results are PUBLIC_API_RUN_COMPLETE or FAIL, never overall/sound PASS. native_audio_verified remains false; root_pcm_capture_required/root_trusted_input_required remain true. Root combines actual 997Hz PCM, suspended/closed quiet, real input evidence and normal-close/full-restoration evidence. Synthetic gates emit GUARDS_PASS_NOT_RUNTIME only.

This C28 delivery derives from the fixed C27 wrapper SHA-256 `744d5df7bd3347f718e90497895299239c70ffdf07252f88ee0b2590c36d10c9`. Production changes are candidate identifier/root/schema changes plus the additional required `Source/WebCore/Modules/webaudio/AudioNodeOutput.cpp` snapshot pin: twelve original-and-new sources, with no original pin removed. Native pins, frozen fixture pins, exact original 128-frame delay vector, and devices.yml preservation remain unchanged. Root's original failures at `/workspace/webaudio-c26-native-01` and `/workspace/webaudio-c27-native-01` remain preserved. C27's delay/channel failure is not reclassified; C28 rebuilds the graph output-cache guard. No C28 image identity or runtime success is claimed here.
