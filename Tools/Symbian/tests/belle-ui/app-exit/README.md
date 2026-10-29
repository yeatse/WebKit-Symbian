# Request normal browser exit during a native dialog

This small native E32 helper uses the published `TApaTask::EndTask()` API. It
finds only browser UID `0xE1A5C002`, checks the process secure ID and
`browser.exe` filename, registers a process logon, then sends the official
application shutdown event. It never calls `KillTask`, process `Kill` or
`Terminate`. There are no command-line arguments or browser test hooks.

Build independently with `bash build.sh`, after setting `SYMBIAN_SDK_PREFIX`
and `SYMBIAN_NATIVE_CONVERTER`. `PROBE_BUILD` selects an isolated build directory;
the script uses one compiler job. Deploy `e32/browser_exit_request.exe` to a
guest `sys/bin` directory. This executable is test infrastructure, not part of
the browser package.

Use a fresh, exclusively owned EKA session with the browser running
`diagnostics.html?case=exit&run=UNIQUE_TOKEN`. Open the real native picker and
save its screenshot and the fresh `ARMED` log. Then use **Emulation → Launch
process** in EKA's Qt UI to launch `C:\sys\bin\browser_exit_request.exe` in
the same emulated system. This menu directly spawns a second guest process;
it does not replace the browser's existing CLI exit callback. Do not start a
second emulator or send a WM-close request.

The helper writes and flushes `C:\Data\WebKit\app-exit-request.log`. Require
its checked TARGET and REQUEST/REQUEST_SENT, the browser's own
`WebKit Browser exiting` destructor log, no panic/AV, and the expected normal
guest exit. A separate validator must handle this expected exit: the ordinary
`run-browser.sh` deliberately rejects early exits and must retain that rule.

In CLI `--run` mode, EKA's `on_app_exited` intentionally exits the host after
the main guest app ends. Host exit 0 alone is therefore insufficient, and the
host is not required to remain alive. It may stop the observer before the
helper writes its optional EXIT/PASS lines; use the browser destructor log
and guest exit evidence to establish the outcome. If the emulator remains
alive and the browser has not exited after ten seconds, the helper reports
FAIL and returns without forcibly terminating it.

Initial build validation: ARM object, ELF link and E32 conversion passed.
The 77,252-byte image SHA256 is
`e668e662f15b563b71e5237b910db422f86fc265de7f6a88925b310776e7489d`.
Runtime application teardown is still to be verified by the sole EKA operator;
building or delivering the request is not a teardown PASS.

`validate.py` checks a completed observation without launching any process:

```sh
python3 validate.py RUN_DIRECTORY \
  --browser-sha256 EXPECTED_BROWSER_SHA \
  --emulator-sha256 EXPECTED_HOST_SHA \
  --helper-sha256 EXPECTED_HELPER_SHA
python3 test-validator.py
```

The directory must contain `launch.json`, `observation.json`, the fresh browser
`log.txt`, `emulator.log`, and `app-exit-request.log` (logs may also be gzip).
The observer must move aside previous guest/helper logs before launch, verify
the installed executable hashes, record the unique page URL, and explicitly
record `wm_close_sent: false` and `forced_cleanup: false`. These are observation
provenance requirements; the validator cannot establish log freshness from
filenames alone. GDB launcher exit 0 is never accepted as a normal host exit.

The 12 host fixture tests cover missing destructor completion or armed scenario, stale page URL,
wrong image/target identity, unsent shutdown, guest panic, abnormal guest exit,
host crash, forced cleanup/WM close, and the permitted absence of the helper's
final EXIT line. The historical candidate5 runtime is correctly rejected; see
the [extra UI evidence](../../../docs/validation-belle/ui/extra-candidate5/README.md).
