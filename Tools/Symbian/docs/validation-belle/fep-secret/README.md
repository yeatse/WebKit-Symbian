# 0029 native FEP secret editor proof (2026-11-01)

The Qt FEP dialog chooses `QLineEdit::Password` when official Symbian editor capabilities report secret text. Ordinary Unicode multiline requests retain `QPlainTextEdit`. This candidate contains the exact 0029 patch, source, tests, complete native DLL ABI and integrated-host deployment evidence. **Real guest UI acceptance has not been performed for this host/FEP pair.**

## Identities

| Item | Identity / result |
| --- | --- |
| 0029 patch | SHA256 `65d65285d3881d66cf25febfb7405cd96afe9eb06a6b6b7a5a6346609103ecc2`; 15 source files; follows 0028 |
| Root integrated host | 65,484,680 bytes; SHA256 `07af513045bf26e200f7d711c682e219ee4c7cecc9e30f2fa3b08732fc1012e3` |
| New native FEP | 99,488 bytes; SHA256 `fd6092f2ad61d39ccb47475229c367f5494fe68a8bd2bd8d0e04dc1bafcdc7ae` |
| DLL ABI | UID2 `0x10009d8d`, UID3 `0x100056de`, capabilities `0`; only export ordinal 1 `ImplementationGroupProxy`; implementation UID `0x101fd65a` |
| Imports | 9 DLLs / 200 slots checked against present native proxy bindings; 32 fepbase slots; cone ordinal 316 `SupportsSecretText` |
| Dispatch | New A6 carries initial text, maximum length, normalized secret flag and request status. Legacy A0 remains. This DLL requires the matching 0029 host. |

`summary.json` ties the identities together. `source/` holds exact patch before/after inputs. `deployment/current-source-check.json` confirms all 15 integrated source hashes and reverse patch check. Binary outputs are absent; hashes and native ABI reports identify the actual retained external artifacts.

## API and behavior

`SupportsSecretText()` returns `iCapabilities & ESecretText`, with `ESecretText = 0x00000100`. Its true value can be 256, so guest code uses `!= EFalse`; a negative control rejects incorrect `== ETrue`. `api/` preserves upstream implementation, repository/revision/blob and header hashes. The existing cone ordinal is used, with no guessed ordinal or stub.

Password echo mode and sensitive/no-prediction hints are set before the initial text enters the line editor. Secret text never enters the multiline `QTextDocument`. Request-local mode/stale-request guards preserve 0028 Cancel/selection behavior; pending initial text is cleared after display, completion or cancellation. Empty Submit remains accepted. The inspected Android/iOS source paths refuse secret requests before plaintext UI; these paths were not compiled or run, and masked mobile UI is not implemented here.

## Evidence and responsibility

The FEP subagent compiled five real host translation units and ran 21 offscreen Qt widget checks, 51 ASan/UBSan assertions extracted from production host state methods, 12 extracted selector-parameter assertions, and 40 cases extracted from guest methods. Guest ARM C++ and assembly compilation exited 0. These checks use public synthetic strings. They do not execute a guest CPU, start/connect a display server, or establish real guest password UI behavior. `host-tests/` and `guest-tests/` retain harnesses, actual commands and complete logs. Historical harnesses keep original workspace paths as evidence; they are not newly portable test commands.

The FEP subagent then fully ran the portable native builder on a filtered external source copy containing 0028 plus guest 0029. `native-build/` holds the exact source closure, recipe, official header/DEF/license, converter 0006 source patch/test evidence, actual ARM compile commands, complete configure/proxy/build logs, native inspector reports, independent EKA static-parser report and every library's ordinal verification. The real native build completed 8/8. Converter SHA256 was `b538f427aefd004d3c4491b5d07a50c02fcab75a132672a8b2ccb2e647657c4a`. The GNU ELF map is a linker map; no extra runtime ordinal remap is needed.

Root performed the later **full integrated host build and deployment**. The first host link failed because the missing library environment could not resolve `-lEGL`; the full failure log is retained alongside the successful retry log after root restored the prior host environment. `deployment/root-build-attribution.json` distinguishes root's reported exit-0 build/deploy from this archive task's independent read-only hashes. Both `build/bin/patch` and `DATAparent/patch` contain the expected five asset identities (ten files checked). This archive task did not build/deploy a host or start a guest/display.

Early reports remain unchanged. `original-candidate/manifest.json` and `guest-tests/validation.json` predate full DLL/host work and correctly say those later steps had not occurred. `native-build/verification.json` retains the read-only observation of the original `8d3877…` FEP after the host build, following the earlier `aad1a4…` identity; later root deployment/current hashes establish final `fd6092…`. The raw observation alone does not establish which copy operation caused the change. Earlier C15/C17 UI results keep their own old host/FEP identities and do not accept this pair.

## Rebuild and deployment

`native-build/recipe` snapshots the previously delivered `Tools/Symbian/eka2l1/fep` builder. It reads `--eka-source`, `SYMBIAN_SDK_PREFIX`, `--work` and `--converter`; it does not require `/workspace`. Use a source tree with 0028 and 0029, an external work directory, official overlay/proxy inputs and converter 0006. The recorded full build used the filtered `native-build/eka-source` source snapshot. Actual command/result files preserve historical paths for provenance. Official EPL copyrights and exact source identities are retained in `recipe/official`.

An ordinary host build copies precompiled patch assets and can overwrite the separate FEP/MMF outputs. Before guest validation, restore and hash-check all five files in both asset directories: `avkonfep_general.dll`, `mediaclientaudio_general.dll`, `mediaclientvideo_v100.dll`, `mediaclientaudio.dll.map`, `mediaclientvideo.dll.map`. Exact hashes are in `deployment/root-deployment.json`. The new DLL needs an A6-capable 0029 host. File existence alone does not establish deployment.

Remaining acceptance is real UI with a synthetic password: confirm masking before any authentication input, then Submit/Cancel/selection and ordinary multiline Unicode on this exact pair. Calibrate empty-dialog geometry first because the secret line editor layout differs. No real passwords, credentials, profiles or clipboard contents are included.

## Archive integrity

`provenance.json` records original paths, bytes/hashes, retained destination hashes and lossless gzip conversion. Raw statuses and failure logs are unchanged. This directory's `SHA256SUMS` covers every file except itself; candidate-level `SHA256SUMS` covers the whole candidate including patch. Only source, text reports and gzip text logs are retained, with no DLL/ELF/objects/import binaries, keys or private configuration.
