# Native file selection and prompt regression

This fixture requires real native UI interaction. It does not override `prompt`,
set an input's files from JavaScript, or substitute a native test hook. A page
that merely loads is not a pass.

Start `python3 server.py --host 0.0.0.0 --port 8782 --evidence /tmp/belle-ui-uploads.jsonl`
on the validation host. Install the browser and its new `browser_ui.rsc` on the
same drive. Copy `上传-你好.txt` unchanged to a user-visible phone directory, then
open this server's `index.html` using the normal browser/network runner and its
normal exclusive emulator lock. The server accepts only a bounded, single-file
multipart upload; it records the received Unicode filename, byte count and
SHA256. No selected file contents are saved by the server.

Follow the eight numbered steps:

1. Accept the unchanged Unicode prompt default.
2. Accept an empty prompt using OK.
3. Cancel a nonempty prompt; JavaScript must receive `null`.
4. Select the supplied Unicode-named file and verify its FileReader contents
   and the actual HTTP multipart upload.
5. Select one file on a `multiple` input. A one-file result is legal; the native
   UI currently offers one file per invocation, not simultaneous multiselection.
6. Cancel the picker after a prior selection. A `cancel` event must arrive and
   the previous File object and contents must remain.
7. Open the child frame's picker, wait at least five seconds before choosing,
   then return. A timer removes the child during the native modal loop. The
   chooser must discard the stale result and the browser must remain responsive.
   If the timer did not run until after the picker closed, this test fails rather
   than claiming to have exercised detachment.

8. Repeat with a child navigation instead of removal. Completion must not attach
   the selected file to the old document.

Success requires `RESULT: belle-native-ui passed=8 failed=0 total=8`, two exact
upload records and normal browser shutdown without a panic or AV. Preserve raw
guest/host logs and native-dialog screenshots. Also manually cancel at the
memory-selection stage and the file-selection stage, try a different drive,
remove a file before accepting, and exit the application with a dialog open.
These manual lifecycle checks are separate from the eight page assertions.

Current scope: single existing local files from phone/internal/removable
storage; directory uploads cancel. `accept` is an advisory HTML hint and is not
yet applied as a native picker filter. Prompt uses an official AVKON text query
with a 4096 UTF-16-unit limit. An oversized default is cancelled without silently
truncating it. Empty and whitespace-only accepted values remain distinct from
Cancel. Native Leave/errors become cancellation. All native result text is
copied before native storage is freed.

Validation status: candidate3 passed all eight checks with real EKA native UI
interaction, two exact uploads and normal shutdown after 180 seconds. The
[portable evidence](../../docs/validation-belle/ui/README.md) retains full logs,
actions, screenshots and the earlier incomplete six-case run. Physical-device
validation and the separate manual cases listed above remain outstanding.

Host checks: `PYTHONDONTWRITEBYTECODE=1 python3 test-server.py -v` verifies actual
HTTP multipart Unicode names and bytes, changed content, size limits, wrong
fields, cross-origin rejection and truncated multipart rejection. These seven
host checks are separate from the eight interactive browser checks.

## Real X11 interaction helper

`native-interact.py` attaches to an already-running EKA process on the exclusive
validation X display. The normal browser runner still owns the emulator lock,
launch, bounded lifetime and cleanup. This helper sends XTest mouse and keyboard
events only. The fixture's `ACTION` lines report actual DOM button coordinates;
they neither click the button nor synthesize native dialog results.

1. Launch `index.html?run=UNIQUE_ASCII_TOKEN` with a sufficiently long observation
   period for the native dialogs, using a new browser run and the normal runner.
2. Identify the emulator PID and calibrate the HTML content rectangle from the
   current screenshot/X11 window tree, excluding the browser URL bar and native
   softkeys. Pass **window-relative** `X Y WIDTH HEIGHT`, not desktop coordinates.
   Coordinates translate with window movement. Window resize or lost keyboard
   focus aborts further input. The test expects an unrotated, uniformly scaled
   portrait display. A prior validation used a 900 × 700 Qt window at root
   `(1,25)`, a 360-pixel-wide phone at root x=271 and a 48-pixel browser URL bar;
   these are calibration clues, not a promised geometry for another run.
3. First run only the three prompts. With the actual calibrated rectangle:

   ```sh
   python3 native-interact.py --pid "$EKA_PID" --guest-log "$LIVE_GUEST_LOG" \
     --run-token "$RUN_TOKEN" --content-rect "$X" "$Y" "$WIDTH" "$HEIGHT" \
     --through-step 3 --output "$EVIDENCE/prompts"
   ```

   EKA's default binding maps F1/F2 to left/right softkeys. The helper clicks the
   real page button and sends F1, F1, F2. It saves native-dialog and result images.
   A partial run records `PARTIAL`, never an eight-test pass.

   Some EKA configurations first show a separate Qt FEP window titled
   `Enter the text`. Submit that editor before accepting or cancelling the
   AVKON query; submitting FEP alone is not a prompt result. After confirming
   its actual layout, add `--fep-submit X Y WIDTH HEIGHT`: the point is relative
   to the FEP window and the dimensions must match exactly. Candidate3 used
   `--fep-submit 200 277 400 300`. The helper requires the same PID, this exact
   title, `WM_TRANSIENT_FOR` pointing to the selected main window, and keyboard
   focus inside the modal. It raises/clicks the modal itself, waits for it to
   disappear, saves the AVKON query screenshot, then sends F1/F2. Other dialogs
   and changed sizes remain errors; general same-PID focus is not sufficient.
   Calibrated native button clicks can replace the softkeys with
   `--prompt-buttons OK_X OK_Y CANCEL_X CANCEL_Y` and
   `--picker-cancel X Y`. These coordinates are relative to the main window,
   unlike the FEP Submit point. The candidate3 screenshots place prompt buttons
   near `(363,330)` / `(539,330)`; the actual memory Cancel click was `(449,470)`.
   Check the current screenshots before reusing these coordinates.
4. Inspect/calibrate the actual native memory/file navigation once. Put only
   genuine key or window-relative mouse actions in a JSON list, for example
   `[{"key":"Return","wait":1.0,"note":"confirmed phone-memory row"}, ...]`.
   A mouse action uses `{"point":[449,269],"wait":1.0}` after its row is
   calibrated. Supported keys are X11 key names; `count` repeats an action and
   `wait` is its settling delay. The example is deliberately incomplete: folder order and
   remembered memory differ between installations. Do not invent a successful
   sequence. Prefer a dedicated visible test directory containing only the
   supplied Unicode-named file. Keep screenshots of the calibrated choice.
5. Resume with `--from-step 4 --through-step 8 --picker-actions picker.json`
   and a new output directory. The same calibrated key sequence is used for
   single-file, `multiple` and both lifecycle cases; the cancellation case
   sends F2 (or the calibrated Cancel click) at memory selection. The lifecycle cases first wait for the real
   `LIFECYCLE` marker while the picker remains open, then choose the actual file.

The helper rejects a step that already started, stale/missing run tokens,
offscreen DOM targets, missing assertions and page failures. Its final status
is `INTERACTION_COMPLETE`, not a complete browser validation result. Require
the eight-assertion raw guest terminal marker, two exact upload records, reviewed
native screenshots and the runner's normal shutdown/panic checks separately.
All actions, window geometry and screenshot SHA256 values are saved in
`interaction.json`; do not overwrite a prior interaction directory.

`PYTHONDONTWRITEBYTECODE=1 python3 test-native-input.py -v` starts a **private**
Xvfb chosen with `-displayfd` and a fake host window. It checks actual XTest
press/release delivery, wrong-PID rejection, bounds and focus loss. It does not
connect to the EKA display or validate a Symbian dialog. Its six checks also
cover explicit FEP modal identity, size validation and actual modal mouse input.

## Separate lifecycle diagnostics

`diagnostics.html?case=cancel|missing|drive|exit&run=UNIQUE_TOKEN` starts directly
with one native picker. It does not replay the prompts or contribute to the
eight-case suite. Restart the fixture server after updating its allowlist.

- `cancel`: enter the file-list level and cancel there. The page requires an
  actual `cancel` event and an empty file list; native screenshots must establish
  that cancellation occurred below memory selection.
- `missing`: prepare a **disposable** `delete-before-accept.txt` in the test
  directory. After its row is visibly listed, remove only that exact test copy,
  record its old identity and verified absence, then try the old row. Dismiss
  any native error and cancel. The page must never receive a chosen File. Its
  cancellation result alone does not prove that the file was removed.
- `drive`: copy the unchanged Unicode sample to another writable user drive,
  choose it through the real dialog, and retain drive/path screenshots. The
  page checks its Unicode basename and exact FileReader contents; it cannot
  infer the native drive from a browser File's privacy-preserving name.
- `exit`: leave the dialog open and request **normal guest browser application
  exit**. This case intentionally has no page PASS. Preserve the pending-dialog
  screenshot and evidence that the guest browser process exited normally,
  without a host WM-close request. With EKA's CLI `--run` mode, its
  `on_app_exited` callback intentionally exits the host with status 0 after the
  guest application ends; that sequence requires explicit guest-exit evidence.
  Use a separate expected-guest-exit validator. The ordinary browser runner must
  continue rejecting an early host exit. A host WM close, killed emulator, or
  picker Cancel does not establish application teardown during the modal call.
  The independent [official EndTask helper](app-exit/README.md) can send that
  application-exit request through the guest window server while the picker
  covers the browser's menu.

Each diagnostic needs a fresh run token and independent normal-close/panic
checks. Do not include their markers in the formal eight-case count.
