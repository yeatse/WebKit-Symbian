# Native clipboard adapter

Checks the production clipboard adapter's text normalization and admission
helpers with a host SDK fixture. Requires a C++ compiler.

```sh
python3 Tools/Symbian/tests/clipboard-native-adapter/run-host.py \
  --build /tmp/clipboard-native-adapter
```

The runner checks Unicode and line-ending conversion, serialized lengths and
clipboard-store limits. It exits successfully when all checks pass. The editing flow is
covered by [Clipboard editing](../belle-clipboard/README.md).
