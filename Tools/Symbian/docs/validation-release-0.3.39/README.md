# SIS 0.3.39 installed-image regression

Three SIS packages were actually installed by EKA2L1 CLI on 2026-10-29. The final accepted attempts reached package-registration and installation-complete markers, then closed the Qt window normally with host exit 0. Installed registry controllers are byte-identical to the SIS controllers; all package UIDs, version `0.3.39`, and all 11 installed payload hashes were checked.

| Artifact | SHA-256 |
| --- | --- |
| browser.sis | `6f6c9749f81081d2337bdb3ae1ca867ec0eacb084eb6ea14333fab4531380e35` |
| jsc.sis | `26d1e5dfa5fbbf6ed4eec9b352fdd9d44a3982c700be6460dc5bdb84b99b11f6` |
| symbian-webkit-icu.sis | `aa0520f96c91926d36df7351625bfa143ec893bbd56ce27e477553855a0b11f3` |
| installed browser.exe | `2db5f84970bfe15bdf107afc7c6a72f52a6acd6d3dce02493796173ea1d9e3cd` |
| installed jsc.exe | `d26cf9cad3c4e47a0d3a559598bd16ba2bfad772dabeb21c17cb8eb211b44bcc` |
| installed icudt70l.dat | `808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d` |

`installation/result.json` contains the exact installed paths, registry identities and payload sizes. `installation/package-verification.json` independently records SIS CRC/controller/payload verification. These file checks complement actual installation; they do not substitute for it.

The private validation driver was derived from `Tools/Symbian/eka2l1/run-browser.sh`. Its only execution changes were an absolute common-script include, replacing the EXE/RSC/ICU/package-test-page copying with checks of all 11 installed files, and explicitly launching `E:\sys\bin\browser.exe`. Additional non-package HTML fixtures were still copied normally. Original deadline, fresh logs, panic checks, screenshot, expected marker and normal WM-close requirements remained. `installation/driver-adaptation.json` identifies both driver versions. Every page therefore used the installed image rather than replacing it with a build copy. All 11 installed payloads were rechecked unchanged afterward; C and E held the same browser bytes, but this driver launched E explicitly.

| Installed-image check | Result | Observation |
| --- | --- | --- |
| Packaged local DOM / JavaScript / Chinese page | PASS | 30 s |
| ICU IDNA, including invalid Bidi/ContextJ rejection | 9/9 | 20 s |
| ARM11 DOM regression | 8/8 | 60 s |
| WebCrypto, including rejected deprecated AES-CFB | 19/19 | 30 s |

Each row completed with normal host exit 0 and no guest panic/access violation. `smoke-result.json` identifies fresh run nonces, screenshots and byte-exact compressed guest logs. Host logs are not vendored; their original hashes and sizes are recorded. Compositing 10/10 and WebGL 9/9 also passed against this installed image with real native EGL initialization/presentation, using the separate graphics evidence maintained under `validation-graphics`.

The emulator executable remained `ff218e2e687c183c993aad2245824354e726f4c8bbefdff9b093b604602e18b1`. Final native-only MMF replacement DLL identities and unchanged release CA are recorded in the post-run inventory. No ROM was modified, and no physical-phone validation is claimed.

Historical failures are retained in `installation/historical-attempts.json`: the first ICU attempt installed the package but the host crashed before GUI readiness; GDB and later plain CLI attempts did not reproduce this startup crash. A separate validation-harness path ambiguity after browser installation was corrected by preferring exact SIS destination spelling; the browser was reinstalled and reverified. Neither failed attempt was counted as a pass.

This installed-image regression does not claim all media audio continuity is resolved. The same candidate's HTTP media controls passed, while the TLS media run had a separately recorded 33.9167 ms video-audio gap despite its API checks passing. See the media validation evidence for that distinction.

To reproduce, first install ICU, JSC and browser SIS through the emulator's `--install` option, select E for executable payloads, and validate the registry and destinations against the package hashes above. Use the normal strict browser runner with copying disabled as described, launch the installed E-drive image, and run the four local fixture URLs recorded in `smoke-result.json`. Do not copy a build image over the installed EXE before claiming an installation regression.
