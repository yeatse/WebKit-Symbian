# Native prompt and file chooser — candidate3

The complete EKA run passed **8/8** page assertions, two real HTTP multipart
uploads, and 180 seconds of observation followed by normal WM close and host
exit 0. Browser image SHA256 is
`d21abb2a9a23ab08295c56d4c075d67e42c95e00f18ef6924cc325414d3e1d76`;
the validation host SHA256 is
`12c275e7f7b3fa8da4fa58ef4d048799db278baaeca7a45d3f78debad3051a4c`.
This is candidate3 on EKA's RM-707 environment, not a physical Belle-device test.

[`candidate3/result.json`](candidate3/result.json) retains the runtime verdict.
The source run is `browser-20261029T185612Z-0lpxn5_k`. The normal repository
`run-browser.sh` supplied locking, fresh logs, bounded observation, panic checks
and graceful shutdown; it was not modified for this test. Its exact source and
the invocation are archived alongside the result. Workspace paths in those
records describe this particular validation environment.

The eight checks cover Unicode prompt defaults, accepted empty text versus
Cancel, Unicode FileReader and HTTP upload, one legal selection on a `multiple`
input, preserving the previous File on Cancel, and chooser completion after
the source iframe is removed or navigated. Both uploaded files contained the
same expected 37 UTF-8 bytes, SHA256
`3f0bad9f38770739709c88025e2233cd6d0d0ad535838be5102a9fd59a57316f`.

All interaction used real XTest mouse events. EKA's separate Qt FEP editor was
submitted first; the AVKON query was then independently accepted or cancelled.
Submitting FEP was not treated as the JavaScript prompt result. Files were
chosen in the actual firmware memory/file dialogs, not assigned through
JavaScript or a browser test hook. The calibrated picker path was
`C:\Data\00-WebKitUI\上传-你好.txt`.

[`interaction/interaction.json`](candidate3/interaction/interaction.json) records
every action, monotonic time and screenshot hash. The complete screenshot
sequence is retained. For the two lifecycle tests, native memory selection was
still open when each invalidation marker arrived; real file selection followed
5.779 and 6.781 seconds after the child button clicks. See
[`lifecycle-audit.json`](candidate3/lifecycle-audit.json) and the
[`detached`](candidate3/interaction/07-native-after-invalidation.png) /
[`navigated`](candidate3/interaction/08-native-after-invalidation.png) screenshots.
The raw guest log contains each lifecycle marker before its corresponding PASS.
The final screenshot shows the finished page; the raw log, not the visible
portion of its scrolling text, establishes the complete eight-case terminal.

Raw guest and emulator logs are gzip-compressed without changing their bytes.
`manifest.json` records every archived file's hash and the decompressed raw-log
hashes. Fixture, input-helper, server and driver snapshots retain the versions
used for this run; subsequent diagnostic additions are separate. The 150-byte
`browser_ui.rsc` is the actual deployed prompt resource, SHA256
`70ed39945ee9853a09f190238dccff9d41fb63712de06a4f90fb163e15ba880a`.

[`candidate3-incomplete`](candidate3-incomplete/summary.json) preserves the first
run as **FAIL / incomplete**, not a product pass: FEP focus calibration consumed
the 300-second observation, six assertions completed and steps seven/eight were
not run. The strict missing-terminal check correctly failed. The fresh retry
did not reuse its assertions or uploads.

The separate [candidate5 evidence](extra-candidate5/README.md) adds real PASS
results for file-list-level cancellation, removal of a disposable file before
acceptance, and another writable drive. Actual application exit with the
picker open failed and remains under investigation; the host crash and GDB
records are retained. [Candidate8](exit-candidate8/README.md) reports the guest
exception correctly after the emulator fix and localizes the browser failure
to control-stack removal during destruction. [Candidate9](exit-candidate9/README.md)
completes the corrected browser teardown but still fails normal process exit:
the native transition adapter waits for a policy cancellation the emulator's
ALF service did not complete. Both timeout histories and the actual guest
stack are preserved. Closing the emulator after the final page is not
application-teardown evidence. The dedicated diagnostic page and its external
stage requirements are described in the
[fixture README](../../../tests/belle-ui/README.md).

The UI currently selects one existing file per invocation, including on HTML
`multiple` inputs. It does not claim simultaneous multiselection, directory
uploads or native `accept` filtering. Prompt defaults over 4096 UTF-16 units
cancel without truncation. None of these results imply Async Clipboard or DOM
touch support.
