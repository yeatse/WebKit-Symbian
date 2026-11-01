# C29: actual CI50 browser payload controlled acceptance

Root's unchanged acceptance.json records a scoped PASS for the actual audited CI50 browser payload on host0032: all fifteen strict public API reports, four real owned-window XTest controls, 997Hz stereo PCM, normal own-PID close, unchanged pins and complete scoped restoration. This is controlled payload deployment/execution, not SIS installer or physical-phone acceptance.

CI50 run41299027063, source886d227f1780298cc17cf99695a3c78c0b7d9ff6, package0.3.50. Browser59863884B SHA30fd5b179a1ca595216c77da1e4bf4561e9d362dcc2dcde9fd4354d4cb1c0429; metadata SHA4869f3ca993e787ab509c5563a15bb618e629d9828ffda026b821d816bac98fb. identity/ preserves actual CI package/source provenance and explicit public asset/harness pins. No image, ELF, DLL, SIS or ZIP bytes are copied.

public/ contains the original reports, four real XTest records, two no-input calibration observations and one visually reviewed screenshot containing only the controlled localhost page awaiting Start. The 360x532 viewport maps the button center to client450,223, then window origin1,25 gives screen451,248. All clicks target the same owned PID/window. The strict fixture validator still returns native_audio_verified:false because JS alone does not establish sound.

pcm/ preserves unchanged analysis/capture/marks/ready metadata. Raw s16le stereo48000Hz PCM remains external at /workspace/webaudio-c29-ci50-pcm-01/audio.raw:4567540B SHAb7ed4045048998e220d33385ab58e13f31ee48073e8c407c9a3b1627f03093bf. The two running windows have997.433Hz peaks, RMS3474.95/3475.07 and approximately99.9996% selected-band power; suspended and closed windows are exact zero. Independent pure offline numerical recomputation matched the original observations without capture, playback or guest execution.

**timing_alignment_verified:false and PCM whole_run_verified:false remain unchanged.** Host-delivery windows cannot bound an unknown constant queue delay, establish exact synchronization or guest latency, or prove seamless pause.

run-summary.json contains only whitelisted scoped booleans. Original result.json, nested restoration/items, profile/configuration, backups, journals, inventory, credentials, environment and host/guest/fixture logs are excluded. Only explicitly known public pin hashes are extracted. No raw PCM is copied.

harness/ contains the exact fixture, CI wrapper/provenance, preservation/normal-close, real-input and finite capture/analyzer sources for reproduction. Their absolute paths are historical references, not an instruction to launch an emulator. A new reproduction is a separate Root-controlled action using the pinned actual CI image/metadata, fresh outputs, bounded monitor capture and four real clicks with at least1.2s stage holds; see harness/wrapper/README.md and harness/pcm/README.md. No runtime, input or deployment was repeated for this archive.

Safe host-only verification: `python3 -B verify_public_archive.py`. This checks stored API/input/calibration/analyzer conditions and hashes, never the wrapper/input/capture entrypoints. External raw identity is checked when available. SHA256SUMS covers every other file; its SHA-256 is the archive root digest. copy-provenance.json binds unchanged copied public/source bytes.

prior-failures-reference.json references the preserved C26/C27 FAIL records in the committed C28 archive and original run directories; no failure is reclassified. AudioWorklet remains hidden; MMF media-element PCM creation rejects with NotSupportedError. Genuine native resource-loss notifications, seamless pause and official EndTask remain unverified.
