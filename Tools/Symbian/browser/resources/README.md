# Native browser dialogs

`BrowserQuery.rh` defines the native query resources used by `browser_ui.rss`
for JavaScript dialogs and HTTP authentication. The browser builds its `WBUI`
resource file with rcomp and loads it from the executable's drive.

Text prompts accept empty input. Authentication uses the native masked
`SECRETED` editor, with `AuthenticationUI.cpp` handling the dialog controls.

Build the browser and resources from the checkout root:

```sh
Tools/Symbian/scripts/build-browser.sh
```

## Source definitions

The structures retain the field layout and defaults from
[Classic UI](https://github.com/SymbianSource/oss.FCL.sf.mw.classicui/tree/dcea899751dfa099dcca7a5508cf32eab64afa7a/classicui_plat/ui_framework_definitions_api/inc)
(`EIKON.RH`, `avkon.rh`) and
[OS Services](https://github.com/SymbianSource/oss.FCL.sf.os.ossrv/tree/1e9520caca186c601dd9768449b86bc72be39a22/lowlevellibsandfws/apputils/inc/BADEF.RH)
(`BADEF.RH`). Constants use the SDK's `avkon.hrh`, `eikon.hrh` and `uikon.hrh`.
The original copyright and EPL-1.0 notices apply to the extracted definitions;
[source-hashes.json](source-hashes.json) records their upstream file identities.

See [Development](../../docs/Development.md) for SDK and resource compiler setup.
