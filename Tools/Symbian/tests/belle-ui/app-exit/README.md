# Browser exit during a dialog

The native helper requests browser shutdown with `TApaTask::EndTask()` while a
native dialog is open. It targets browser UID `0xE1A5C002`.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native
export PROBE_BUILD=/tmp/browser-exit-request
bash Tools/Symbian/tests/belle-ui/app-exit/build.sh
```

Deploy `e32/browser_exit_request.exe` from the build directory to `C:/sys/bin/`.
Open the Belle UI fixture's `diagnostics.html?case=exit&run=exit1`, open its file
picker, and launch the helper through EKA2L1's **Emulation → Launch process** menu.

`C:/Data/WebKit/app-exit-request.log` records the shutdown request. The browser
completes its destructor and exits normally. `test-validator.py` checks the
saved-observation validator with host fixtures.
