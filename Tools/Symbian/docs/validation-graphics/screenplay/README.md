# ScreenPlay public evidence

Recorded on 2026-11-04. The pinned host0038 native API run passes two surface
sizes/four submissions, Available, Finish and cleanup. The separate C33 browser
run on measurement host0039, harness v9, accepts forced-ScreenPlay display:
page 9 pass/1 WebGL skip; 14 fixed owned color regions, 7200 sampled pixels,
zero mismatches; one ScreenPlay tag and no bitmap fallback. Normal owned close,
independent host exit, audit, original-state restoration and task-private
cleanup pass without force. See [SCREENPLAY.md](../../SCREENPLAY.md).

`browser-c33/host0039-v8-01-result.json` remains a whole-run FAIL because private
cleanup failed, despite passing functional observations. Earlier browser/native
failures and fixture v1 are preserved. Fixture v2 fixes animation-start timing
without changing the expected rectangle/ROIs or its 9s watchdog.

`browser-c33/measurement-tools/` preserves the final v9 public controller,
strict audit, fixture, ownership helper and host-test source. No helper/browser
binaries, private catalog/config, ROM, raw runtime logs or screenshots are
included. The source references the original task layout; it is evidence, not a
self-contained executable deployment. `measurement-host39/` records the external
Qt fixed rendering-child title label; independent process/XRes ownership is
still required, and this measurement label is excluded from production EKA.
`ownership-host-contract-summary.json` summarizes four separate non-guest
Linux/Xlib/Qt checks. `default-auto/` is only a TU compile result.

C33 includes original local IAP/CookieStore/multi-file-upload WIP and a 383-byte
UI resource. It is not the pure CI package. Frozen build-time metadata stays
unchanged in meaning; later runtime acceptance has its own record.

The archive also retains SDK checks, presenter 47/298 UBSan, licensed client
8/75 host/ARM evidence, original v3 EKA source and the final scoped 20-path patch.
Require the patch's before hashes; the upstream commit alone is insufficient.
IAP/private-COW deltas are excluded. The Finish polling/fence budget is 2s;
a bounded host counterexample confirms kernel-lock reacquisition can exceed
that budget under stock renderer queue backpressure.

This accepts the recorded native API and local forced-ScreenPlay display
subset. It does not accept trusted input, default EGL/MMF transitions, full
native browser resizing, phone, vsync, zero-copy, power or performance.
Records omit volatile identifiers and raw private/runtime data. Two source
headers retain original cp1252 punctuation for source identity; fixed host
adapter test logs are public count-only output. This documentation task did not
execute/deploy a guest or query a display.
