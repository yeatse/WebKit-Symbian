# Symbian tests

Run the host smoke suite from the checkout root:

```sh
bash Tools/Symbian/tests/run-smoke.sh
```

[Testing](../docs/TESTING.md) covers setup, browser suites and result files.
Module guides contain the focused commands and expected behavior.

| Area | Guide |
| --- | --- |
| Installation | [WebKit package](sis-package/README.md) |
| JavaScript and DOM | [ARM11 DOM](arm11-dom.md) |
| Graphics | [Browser graphics](browser-graphics-runner.md), [frame-rate benchmarks](benchmarks/README.md) |
| ScreenPlay | [Presenter](screenplay/README.md), [Surface Update](screenplay-update/README.md) |
| Networking | [HTTP and TLS](network-tests.md), [cookies](cookie-samesite/README.md) |
| Media | [Audio and video](media.md) |
| Storage | [Browser storage](belle-storage/README.md), [IndexedDB quota](idb-quota/README.md) |
| Input and UI | [Touch](belle-touch/README.md), [clipboard](belle-clipboard/README.md), [browser UI](belle-ui/README.md) |
| Native services | [Memory](native-memory/README.md), [filesystem](native-filesystem/README.md), [run loop](runloop-wakeup/README.md) |
| Cryptography | [RSA](rsa/README.md), [EC](ec/README.md), [WebCrypto page](webcrypto.html) |
