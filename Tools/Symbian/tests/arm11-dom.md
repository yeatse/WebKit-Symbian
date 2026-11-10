# ARM11 DOM integration

The browser page checks DOM node relationships, attributes, events and
collections while JavaScript runs on ARM11. Configure EKA2L1 using
[Testing](../docs/TESTING.md).

```sh
EKA2L1_EXPECT_PASS='ARM11-DOM DONE PASS 8/8 passed; 0 failed' WAIT=90 \
  Tools/Symbian/eka2l1/run-browser.sh /path/to/browser.exe \
  'file:///C:/Data/WebKit/arm11-dom.html?autorun=1'
```

The page reports each DOM group and finishes with the expected PASS line.
The runner saves the browser and emulator logs.
