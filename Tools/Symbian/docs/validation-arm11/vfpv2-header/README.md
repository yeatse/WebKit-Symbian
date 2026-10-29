# VFPv2 image metadata and final-package recheck

The final JSC and browser images explicitly declare their VFPv2 hardware requirement in the E32 header (`0x12100028`). This corrects the image metadata; it does not claim to fix a demonstrated register-preservation failure. No physical phone was tested.

[Independent verification](verification.json) records that the only changed bytes are the four header CRC bytes at offsets 20–23 and the floating-point flag byte at offset 46. Every byte from `iCodeOffset` (156) to the end of each image is identical to the previously validated image. The CRC was recomputed independently, repeated application is idempotent, and the default header-editor invocation preserves the flags.

| Image | Previously tested SHA-256 | Final SHA-256 |
| --- | --- | --- |
| Browser | `b169b7901a7171559becd0928998f03fa793b15ed86de88a97e61f505acbbb97` | `8bdfedccfe9daac342603733f7d714d11987af39f83a9da7d54af7e3e8e1cacd` |
| JSC | `86fc45d8ccf82c39c3519366f8633c5d1465aeb742dd3ee1404ad8dea5c448a7` | `f6358eda691be2cc92cfa63a900d7cc42f8f9b2965ae1ad0c06cb4de6ebba6dd` |

Both final SIS packages were actually installed onto E using the same frozen EKA2L1/X7 environment. Installation-complete and package-registration events were observed; installed EXEs, resources, scripts and package registry entries were checked. The [browser](sis/browser.json) and [JSC](sis/jsc.json) records include hashes and normal host exits. C and E contain the same final images. The unchanged ICU package and installed data retain their earlier verified bytes. [SHA256SUMS](sis/SHA256SUMS) identifies the final three packages.

The final images then passed these additional checks:

| Check | Result | Evidence |
| --- | --- | --- |
| Browser local DOM, JavaScript and Chinese text, 30 seconds | PASS | [log](local/log.txt), [screenshot](local/browser.png), [result](local/result.json) |
| Browser DOM getters, mutation, wrappers and allocation pressure, 90 seconds | 8/8 | [log](dom/log.txt), [screenshot](dom/browser.png), [result](dom/result.json) |
| JSC default concurrent DFG and GC | PASS | [log](concurrent/jsc.log), [result](concurrent/result.json) |

Both browser runs closed normally with host exit zero. JSC reported `jsc exiting 0` and host exit zero. No guest panic or access violation was observed. The concurrent run used one compiler worker and default thresholds: both kernels compiled once and each executed 128 times in DFG; two collections started and returned within DFG, and all 32 retained roots survived. The browser screenshots were inspected, and the release CA hash remains `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`.

The earlier [complete browser regression](../browser/README.md), [three-tier JSC matrix](../eka-jsc/result.json), and [existing JavaScript/Intl/buffer regression](../eka-regression/result.json) are preserved under their original image hashes. They establish the behavior of the identical executable payload; WebCrypto and network suites were not rerun solely for this header correction. The [manifest](result.json) links the final installation and recheck evidence to those prior records.
