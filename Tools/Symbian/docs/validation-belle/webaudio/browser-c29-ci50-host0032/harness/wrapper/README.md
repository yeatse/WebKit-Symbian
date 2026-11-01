# C29 actual CI browser payload wrapper — prepare only

This external wrapper derives from the fixed C28 wrapper SHA-256 `6b6561bb55135cff37461541c9ba3256568899ad60c8de92869142e44ec2deff`. Preparation does not run GitHub tools, download artifacts, build, deploy guest files, start a server/guest/display/audio, or alter shared source/index/commits. Existing C28 images and all prior FAIL/PASS records stay unchanged.

The actual CI50 source commit is `886d227f1780298cc17cf99695a3c78c0b7d9ff6`; run `41299027063` and job `124825909684` were supplied by Root's CI monitor. Browser/package/artifact/resource hashes remain pending until the actual completed CI audit. Templates contain null pending values rather than invented image identities.

The thirteen committed source snapshots have already been exported with `--sources-only` to `candidate29/source`. Their source-only report is `/workspace/symbian-belle-work/candidate29/source-identities-ci.json`, SHA-256 `b886b838bb21f7b07aa0d5be6fe276c0e550e94454e99026fdc026b5ea2013ef`. This is exact Git-object evidence, not a claim that the pending CI package passed. No browser or resource payload was materialized by this step.

## Preserved runtime behavior

The original strict eight-file fixture pins (`9b60cc00760a5a1590b741741fc6edb473feef069cac533243c9bebc83362db7`), original public offline vectors, and Start→Suspend→Resume→Close real trusted-input flow are unchanged. Root independently controls actual XTest input and bounded native PCM capture. The fourteen native copy pins, host0032, dual locks, disabled cores, whole WebKit profile/six-resource isolation, config/bindings/Qt/devices.yml preservation, own-PID normal close, fresh health/nonzero-guest gate, deadline/forced-cleanup rejection, unchanged inputs, and full restoration remain required.

Only the provenance and resource inputs change. The wrapper no longer accepts local CMakeCache, cmakeconfig.h, or generated Settings as evidence for the remote build. Its six deployed sources—browser, three RSCs, ICU, and fonts—must match actual verified CI payloads. UI must be the actual 242-byte Auth255 resource; no C28 383-byte Multi resource hash is retained. Registration/localization identities also come from the actual package, without assuming previous hashes.

## CI provenance inputs

`audit-contract-template.json` defines the Root-provided audit contract. Its eight path/SHA report roles are `run`, `artifacts`, `verification`, `archive`, `package`, `sis_payload`, `auth_resource`, and `e32_imports`. The first seven follow the actual CI49 report formats; CI50 must supply new actual reports bound to this run/commit/version. All three SIS and all thirteen exact payloads, their sizes/hashes/CRC audits, the actual four-member artifact ZIP/digest, actual Auth255 source/resource audit, and all materialized files are required. A summary PASS alone is insufficient.

The E32 report template requires `method: parsed-e32-import-table`, binds the exact extracted browser size/SHA, and records canonical DLL names including `mediaclientaudiostream.dll`. Binary string scanning, a local proxy, or a link log cannot substitute for an actual import-table audit.

The twelve required audio sources plus `Source/cmake/OptionsSymbian.cmake` are obtained only by `git show` of the explicit actual CI commit. Snapshots record the Git blob, SHA-256, bytes, and commit under `candidate29/source/<relative>`. The actual source Options must default WebAudio ON; Audio settings/worklet disable/gesture policy are checked from the committed source. These snapshots can legitimately differ from dev files with Cookie/Multi WIP. No comparison to the working tree or automatic HEAD fallback is used.

Root's package auditor materializes the verified `browser.exe`, `browser_ui.rsc`, `browser_reg.rsc`, `browser_loc.rsc`, `icudt70l.dat`, and `fonts.conf` in `/workspace/symbian-belle-work/candidate29`. The helper verifies them against the actual package and does not install SIS packages or manufacture a payload. Missing reports, ZIP, three SIS, any of thirteen payloads, materialized assets, Git objects, or source snapshots refuse execution.

## Commands after actual CI audit

Default mode is prepare-only and creates no proposed runtime output:

```sh
python3 -B /workspace/symbian-webaudio-classic-runtime-c29-ci50-work/run.py \
  --output /workspace/webaudio-c29-input-check-01
```

Root can export only the known committed sources, independently of pending artifact results:

```sh
python3 -B /workspace/symbian-webaudio-classic-runtime-c29-ci50-work/freeze_metadata.py --sources-only
```

After Root supplies actual audit/package/payload files and actual digests:

```sh
python3 -B /workspace/symbian-webaudio-classic-runtime-c29-ci50-work/freeze_metadata.py \
  --audit-contract "$CI50_AUDIT_CONTRACT" --audit-contract-sha256 "$CI50_AUDIT_SHA256"
```

The helper prints the frozen runtime metadata digest. Root supplies that and the audited browser digest; no placeholder or local C28 hash is accepted as CI evidence:

```sh
. /workspace/eka-validation/env.sh
. /workspace/symbian-media-work/env.sh
python3 -B /workspace/symbian-webaudio-classic-runtime-c29-ci50-work/run.py \
  --execute --browser /workspace/symbian-belle-work/candidate29/browser.exe \
  --browser-sha256 "$C29_ACTUAL_CI_BROWSER_SHA256" \
  --candidate-metadata /workspace/symbian-belle-work/candidate29/runtime-metadata.json \
  --candidate-metadata-sha256 "$C29_CI_METADATA_SHA256" \
  --output /workspace/webaudio-c29-ci50-native-01
```

The server is the original finite localhost8864 fixture; each execution has a fresh token/output. The wrapper result is `CI_BROWSER_PAYLOAD_RUN_COMPLETE` or `FAIL`, always `native_audio_verified: false`. It supports only controlled deployment/execution of the actual audited CI browser payload under the pinned emulator/native assets. Root still combines independent PCM and trusted-input evidence. It never establishes SIS installer operation or physical phone acceptance.
