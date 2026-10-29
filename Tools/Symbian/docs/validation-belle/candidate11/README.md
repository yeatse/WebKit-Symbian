# Second Belle browser candidate

The full engine and browser build completed successfully. The candidate image is
58,792,332 bytes, SHA-256
`20f58cef43d6fa4252546cb337940bfa609ba7ef71d0059ef1336bd126ef5288`.
Its 192-byte `browser_ui.rsc` includes the new native authentication resource;
do not mix this image with candidate 10's resource.

The [source snapshot](source-identities.json) records all 88 changed, added or
deleted runtime paths against `df7274dac90`. The intervening commit after the
initial build only added emulator patches and evidence; runtime source was
unchanged. The build resumed after an interrupted session and later used the
freed fourth CPU slot. A missing native ConnMon header include was corrected
before the successful complete build. The final archive and browser build logs
are retained as unmodified gzip streams.

[Build identities](build-identities.json) record the actual E32/resources and
archive hashes. The static archives are thin: their hashes cover archive
metadata, while the E32 hash identifies the complete linked program. CMake
caches retain the actual target configuration. SDK, firmware and binaries
remain outside this documentation archive.

This candidate includes download, touch, RSA/EC, HTTP origin authentication,
system clipboard, connection observation and memory-pressure integration.
The external CookieStore, IPv6/HTTP2 and IAP proxy work is not included.
Build success is separate from each feature's runtime acceptance:
[RSA 33/33](../rsa/browser-c11/README.md) and
[EC 30/30](../ec/browser-c11/README.md) passed. Other candidate tests are pending.
