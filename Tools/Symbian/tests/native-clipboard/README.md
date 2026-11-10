# Native clipboard

Builds a Symbian `CClipboard` and `CPlainText` probe for Unicode text round trips.
Select the SDK and converter used for the browser.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/sdk
export SYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native
export PROBE_BUILD=/tmp/native-clipboard
Tools/Symbian/tests/native-clipboard/build.sh
```

Run `e32/clipboard_probe.exe` from the build directory with the `write` argument,
then run it again with `read`, using the native probe setup in
[Testing](../../docs/TESTING.md). Passing results preserve the exact fixture text
across the two launches. Each launch writes `C:/Data/WebKit/clipboard-probe.log`.
Save clipboard contents needed after the test and restore them when finished.
