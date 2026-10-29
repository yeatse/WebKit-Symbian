# Browser DOM regression page

Open `arm11-dom.html` and select **Run checks**, or add `?autorun=1`. It uses ordinary web APIs, finite loops and bounded allocation batches; it needs no network service or test-only JavaScript globals.

The eight groups cover hot Node relationships and node types, replacement/adoption, Element attributes and live collections, Document root replacement, Event getters during and after dispatch, isolated wrapper prototype changes, invalid receiver rejection, and live wrappers across allocation pressure. Allocation pressure does not guarantee a garbage collection. A passing page does not establish which JSC tier executed; use the separate ARM11 JSC matrix for tier evidence.

The EKA browser runner copies the page into the guest automatically. With the existing EKA/display environment configured:

```sh
EKA2L1_EXPECT_PASS='ARM11-DOM DONE PASS 8/8 passed; 0 failed' WAIT=90 \
    Tools/Symbian/eka2l1/run-browser.sh /absolute/path/to/browser.exe \
    'file:///C:/Data/WebKit/arm11-dom.html?autorun=1'
```

Require all eight `ARM11-DOM PASS` group markers and exactly one successful `ARM11-DOM DONE` marker in fresh logs, with no `ARM11-DOM FAIL`, panic, timeout or forced emulator shutdown. The page also exposes `{state, total, passed, failed}` as `window.testResult` for a host browser harness. Host browser success is a test-logic check, not ARM/Symbian execution evidence.
