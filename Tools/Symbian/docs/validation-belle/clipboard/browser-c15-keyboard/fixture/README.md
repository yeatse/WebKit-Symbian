# Belle clipboard integration acceptance

This page requires real native input and the production system clipboard
adapter. It neither grants DOM paste permission nor substitutes a browser-private
clipboard. The nine-case report is **not yet a Symbian guest result**.

Start the fixture server outside the checkout's evidence directories:

```sh
python3 Tools/Symbian/tests/belle-clipboard/server.py \
  --bind 0.0.0.0 --port 8802 --reports /external/clipboard-reports
```

Open `http://localhost:8802/index.html?run=UNIQUE_TOKEN&mode=keyboard` through the
normal exclusive browser runner. Use a fresh ASCII run token, 1–80 characters.
`mode=menu` runs the same assertions using the actual Options menu instead of
keyboard shortcuts. Run both modes separately if claiming both routes work.
The page cannot distinguish a menu-generated paste event from a native keyboard
paste solely by its DOM result; retain the actual native-action ledger and
screenshots. Keyboard mode additionally requires trusted control-key events.

Before either mode, the runtime owner must preserve the emulator's existing
clipboard store bytes/metadata, or its original absence, and restore that state
in `finally`. Resolve the actual clipboard drive through the native HAL/probe;
do not assume C. The known X7 profile uses `D:/System/Data/Clpboard.cbd`. The
fixture necessarily replaces clipboard contents with its own known text.
Keep any original store in private external storage, never in Git or published
logs, and do not read/print its previous text. This page is not a substitute for
the separate native adapter's corrupt-store, locked-store, capacity and error
propagation checks.

## Genuine interaction

Each case starts with a real pointer click on **Start**. JavaScript sets the
fixture text, focus and selection, then observes the browser's real editing
operations. `ACTION: belle-clipboard` JSON announces the run, case number,
requested operation, focused element and current DOM coordinates. These lines
do not execute an operation. A host driver may use the existing XTest helper's
PID/focus/geometry checks or an equivalent audited real-input path; do not use
`element.click()`, synthesize clipboard events, set destination text, inject
clipboard data, or call `execCommand('paste')` to satisfy a positive case.

Follow each announced operation:

- `start`: click the visible Start button. A synthetic click fails its
  `isTrusted` assertion.
- `paste`: send actual Ctrl+V, or open Options and choose Paste. The selected
  destination is already focused; avoid clicking it again and moving the caret.
- `copy` / `cut`: send Ctrl+C / Ctrl+X, or use the corresponding Options item.
- `select-all`: send Ctrl+A, or use Options → Select all. Wait for the next
  requested Copy action before continuing.

Each native operation has a 45-second deadline. The entire page has a
480-second deadline, including operator pauses. Missing input cannot produce a
PASS. A failure is retained and the page advances to the next case. The source
and destination rectangles may change between textarea and contenteditable
cases, so use the fresh ACTION coordinates rather than a stale screenshot.
If the emulator presents its separate Qt FEP editor, inspect/dismiss it using
its genuine UI; do not assume keyboard focus is still in the browser widget.

## Exact nine-case contract

1. A trusted button uses DOM Copy on a selected Chinese/Latin/emoji multiline
   textarea. Native Paste must insert the exact text, including LF and trailing
   LF, at the selected destination range without modifying the source.
2. Trusted DOM Cut removes only the selected substring. Native Paste recovers
   exactly that substring, including a surrogate-pair emoji.
3. A styled contenteditable selection copies through the native plain-text
   bridge. Native Paste into another contenteditable inserts the exact text
   without restoring bold/italic markup or exposing an HTML clipboard type.
4. **DOM paste denial is independent.** Even in a genuine click handler,
   `document.execCommand('paste')` must return `false`, dispatch no paste/input
   event, and leave the destination unchanged. No permission is granted for
   this test. Native Paste in the other cases remains a positive path.
5. Password Copy/Cut commands are disabled under the current WebCore command
   policy. No password copy/cut event or deletion occurs; a subsequent native
   Paste proves that the earlier known clipboard sentinel remains unchanged.
6. Readonly Cut is disabled, leaves the source intact and does not overwrite
   the known clipboard sentinel, which is then pasted natively.
7. Genuine native Copy followed by native Paste recovers exact Unicode text.
8. Genuine native Cut deletes exactly the selected range; native Paste recovers
   its exact text.
9. Genuine native Select all selects the entire field from an initial collapsed
   caret; native Copy/Paste then round-trips the whole field.

Positive paste groups require one trusted paste event, exact known plain text,
no file data, only the supported plain-text MIME types, and a normal trusted
`insertFromPaste` input event. Event targets inside contenteditable markup are
associated with their owning test editable; the actual target node name is also
recorded. Reports contain comparison outcomes/lengths, not unexpected clipboard
contents.

## Reports and validation boundaries

The page emits one JSON `RESULT: belle-clipboard ...` and posts that same report
to `/report`. The server requires its own Origin and saves
`RUN-keyboard.json` or `RUN-menu.json` exclusively; a duplicate receives HTTP 409
and cannot overwrite an earlier failure. Failed complete reports are retained.
No report file is served back to a page.

```sh
python3 Tools/Symbian/tests/belle-clipboard/verify.py \
  --report /external/clipboard-reports/RUN-keyboard.json \
  --guest-log /external/fresh-jsc-or-browser-log.txt \
  --run RUN --mode keyboard --origin http://localhost:8802
```

This validator requires all nine distinct cases in order, trusted event
observations, exact counts, bounded elapsed time, one matching fresh console
report and no failure marker. Its PASS covers only the page/report contract.
Also require the production browser/SDK/host image identities, genuine action
ledger, reviewed screenshots, normal guest/host shutdown without panic/AV,
and verified restoration of the original clipboard store. A native probe using
CClipboard alone does not establish these browser/editor paths; conversely,
this page does not establish another application's clipboard interoperability
or native disk-error handling. Keep those independent evidence scopes separate.

## Host checks

```sh
node --check Tools/Symbian/tests/belle-clipboard/suite.js
PYTHONDONTWRITEBYTECODE=1 python3 Tools/Symbian/tests/belle-clipboard/test-contract.py -v
```

Eight host checks exercise the report contract and a real temporary HTTP server:
trusted/negative case separation, stale identities, missing/reordered cases,
incorrect summaries and deadlines, missing paste/input/key observations,
immutable report storage including failures, origin rejection and file allowlist.
These checks never access EKA or a native clipboard.

A separate local Chromium sanity run used genuine browser key input to exercise
the page sequence. Chromium is not the target plain-text bridge: its rich-text
clipboard includes HTML, and its password/readonly `execCommand` return/event
behavior differs from WebCore's current disabled-command policy. Those groups
remain failures in that host run; the fixture is not weakened to make another
browser satisfy the Symbian contract. No nine-case guest success is claimed.
