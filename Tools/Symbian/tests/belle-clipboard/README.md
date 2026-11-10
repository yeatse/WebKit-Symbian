# Clipboard editing

Checks Unicode text copy, cut, paste and select-all in textareas and editable
content through the native keyboard or Options menu. The fixture uses the system
clipboard; save any clipboard contents needed after the test.

```sh
python3 Tools/Symbian/tests/belle-clipboard/server.py \
  --bind 0.0.0.0 --port 8802 --reports /tmp/clipboard-reports
```

Open `http://localhost:8802/index.html?run=clipboard1&mode=keyboard` in the browser.
Click **Start** and follow each requested keyboard action. Use `mode=menu` for the
Options menu flow. The page displays the result and posts its report to the
server's report directory.

The text round trips, editing events and selection checks pass together. Restore
the saved clipboard contents after completing the test.
