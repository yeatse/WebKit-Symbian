# ARM11 JIT browser validation

The complete WebCore/WebKitLegacy browser was built, converted to E32, installed from SIS, and tested on EKA2L1 with the Nokia X7-00 RM-707 firmware. All runs used the default JIT configuration. The [manifest](result.json) records the image, emulator and final package hashes.

| Check | Result | Evidence |
| --- | --- | --- |
| Local DOM, JavaScript and Chinese text | PASS | [log](local/log.txt), [screenshot](local/browser.png) |
| DOM getters, mutation, wrappers, prototypes and allocation pressure | 8/8 | [log](dom/log.txt), [screenshot](dom/browser.png) |
| WebCrypto vectors and API rejection cases | 19/19 | [log](webcrypto/log.txt), [screenshot](webcrypto/browser.png) |
| HTTP, XHR, cookies, redirects and WebSocket | 23/23 | [report](network/report.json), [log](network/log.txt), [screenshot](network/browser.png) |
| HTTPS/WSS trust and hostname checks | 6/6 | [report](tls/report.json), [log](tls/log.txt), [screenshot](tls/browser.png) |

Every observation completed with normal window closure and host exit zero, without guest panic or access violation. DOM was observed for 90 seconds, network and TLS for 60 seconds each, and local/WebCrypto for 30 seconds each. The screenshots were inspected. Network reports carry fresh per-run tokens, and TLS includes successful connections plus explicit wrong-hostname and untrusted-root rejection. The temporary test root was removed, restoring the release CA byte-for-byte to SHA-256 `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`.

Both [browser.sis](sis/browser.json) and [jsc.sis](sis/jsc.json) were installed through the emulator's actual installer onto E. Registration and installation-complete events were observed, the package registry was checked, and installed EXE/resources/test files were compared byte-for-byte with their build inputs. C and E contain the same final browser and JSC images. The [ICU package](sis/icu.json) is byte-identical to the previously installed package; its installed data was rechecked without claiming a fresh installation.

The final browser SIS contains the neutral `Symbian^3 · ARM11` sample-page label shown in the local screenshot. This asset-only repack leaves the browser executable unchanged at SHA-256 `b169b7901a7171559becd0928998f03fa793b15ed86de88a97e61f505acbbb97`.

These records retain the image and package hashes used for this complete regression. The subsequent [VFPv2 header correction and final-package recheck](../vfpv2-header/README.md) documents the final published hashes, actual reinstallation and additional local/DOM/concurrent-DFG runs. Its executable payload is byte-identical to the images recorded here.

These page checks verify browser behavior. Actual LLInt/Baseline/DFG execution and OSR evidence are retained separately in [the JSC matrix](../eka-jsc/result.json). This validation did not exercise a physical phone, WebGL, or media playback.

To reproduce, build and package the default `dfg` mode, install the SIS files using the configured firmware's XDG directories, and use `eka2l1/run-browser.sh` with the listed test pages and markers. [Network fixture instructions](../../../tests/network-tests.md) cover HTTP/TLS setup and the strict network runner. Use the workspace's shared emulator locks and retain normal host-exit checks; an output marker alone is insufficient.
