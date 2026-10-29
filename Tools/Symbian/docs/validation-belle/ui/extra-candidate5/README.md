# Additional native picker cases — candidate5

Three additional real-input cases passed: Cancel from the native file list;
selecting the Unicode sample on writable drive E; and selecting a listed file
after the external test driver removed that disposable file. The last case
returned native error -1 and a cancelled/empty selection, not a nonexistent
upload. Each observed run ended normally with host exit 0 and no guest panic
or access violation. These cases supplement the earlier eight-case UI suite.

Browser SHA256:
`139e140d0802e9de9b202e6bb3d292ad7507415d68fbe86979f5af5f6756a7ff`.
Validation host SHA256:
`5f8704eb1c2461ceeb160eabd927a4f828765cad353e9107ae43f463127e292a`.
This is EKA's RM-707 environment, not physical-device validation.

[`result.json`](result.json) uses local archive paths. The unchanged source
manifest is [`source-result.json`](source-result.json); its absolute workspace
paths identify the original environment. The original guest and emulator log
bytes are gzip-compressed. `manifest.json` records compressed and original
hashes. XTest action JSON, screenshots, drive sample identity and deletion
records are retained alongside the logs. Historical observer scripts contain
environment-specific paths and are provenance snapshots, not generic runners.

## Application exit did not pass

[`exit/observation.json`](exit/observation.json) records host SIGSEGV (-11)
after the official EndTask helper verified browser UID/process/executable and
flushed its shutdown request. The native picker returned Cancel; the browser
did not reach its final `WebKit Browser exiting` marker. No WM-close request
or forced process cleanup was used. This remains **FAIL**. Cancellation of the
picker, delivery of the shutdown request, or GDB's own exit 0 is not successful
application teardown.

The historical page also logged `FAIL` merely because the picker returned
Cancel. That page-level expectation was too strong: official `CEikShutter`
first sends Escape to dismiss dialogs, then queues `EEikCmdExit`. The fixture
now logs that cancellation neutrally and waits for external shutdown evidence.
This does not change this run's FAIL verdict: host -11 and the missing final
destructor marker independently establish unsuccessful teardown. Original
logs and their hashes remain unchanged.

The two GDB reproductions are retained in `exit-gdb/` and `exit-gdb-context/`.
The latter identifies guest `uiklaf.dll` runtime code base `0x855ea000`.
Its recorded PC `+0x7e2` is the start of a translated block in
`LafEnv::MatchFont` (export ordinal 41); the block calls the font-array helper
at `+0x4b8`. The failing guest instruction is `ldr r5, [r0, #4]` at `+0x4c2`,
with `r0 = 0`. The emulator's empty TLB entry incorrectly permits that guest
null read to become a host null read. Correcting that emulator fault handling
does not establish correct browser teardown.

At this point the missing final marker does not establish whether the browser
destructor had started. The actual caller that requested a font with a null
array is still under investigation. Browser exit-command and destructor-stage
logs were added for a subsequent candidate; this archive predates them.

The official reference mapping uses
[`LafEnv::MatchFont`](https://github.com/SymbianSource/oss.FCL.sf.mw.classicui/blob/dcea899751dfa099dcca7a5508cf32eab64afa7a/uifw/uiklaf/src/lafenv.cpp),
the corresponding
[`uiklaf` export DEF](https://github.com/SymbianSource/oss.FCL.sf.mw.classicui/blob/dcea899751dfa099dcca7a5508cf32eab64afa7a/commonuisupport/uilaf/eabi/uiklaf_unbranchedu.def),
and actual firmware bytes, not assumed symbols from a different build. No
firmware binary is redistributed here.
