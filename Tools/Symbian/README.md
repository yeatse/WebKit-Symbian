# WebKit for Symbian^3

A modern web engine for Nokia's last Symbian phones. This is WebKit 2.32.4 (2021), ported to Symbian^3, Anna and Belle phones (Nokia N8, E7, C7, X7, 808 PureView and the like, ARM11 processors). It brings them a browser and a JavaScript engine from the current web era, ten years after their own.

![The test page in EKA2L1](docs/browser-eka2l1.png) ![A select element's native list](docs/browser-select.png)

## What's in it

* **WebKit Browser**, a native touch application:
  * pages drawn by a current WebCore: CSS3 (gradients, rounded corners and the rest), Chinese and other scripts in the phone's own fonts
  * the phone's own controls for menus, `<select>` lists and JavaScript alerts
  * text fields edited with the phone's input methods, Chinese pinyin included, inline as in any native editor
* **JavaScriptCore** with a just-in-time compiler for the phones' ARMv6 processors, which upstream WebKit dropped in 2018:
  * all of ES2020: optional chaining, `??`, `BigInt`, `Promise.allSettled`, `globalThis`, modules…
  * `Intl` (dates, numbers, collation, plural rules, word segmentation) in 20 languages
* **jsc**, the JavaScript shell, for developers.

## Download

* **Releases**: tagged versions (`symbian-vX.Y.Z`) are published on the [Releases page](https://github.com/yeatse/webkit-symbian/releases).
* **Latest build**: every change is built by GitHub Actions. Open the latest successful [Symbian workflow run](https://github.com/yeatse/webkit-symbian/actions/workflows/symbian.yml) and download its `sis` artifact (needs a GitHub login).

| Package | Contents |
| --- | --- |
| `symbian-webkit-icu.sis` | Language data both programs need (`Intl`, text handling). Install it first, on drive C:. |
| `browser.sis` | WebKit Browser, with a test page in `C:\Data\WebKit\test.html` |
| `jsc.sis` | The jsc shell and test scripts in `C:\Data` |

## Installing

1. Install `symbian-webkit-icu.sis`, then `browser.sis` (and `jsc.sis` if you want the shell). The browser and jsc can go on any drive.
2. The packages are unsigned. A stock phone's installer accepts only signed packages; install them on a phone that allows unsigned installation, or sign them with your own certificate (see the [developer guide](docs/Development.md#sis-packages)).
3. Upgrading from an older build: newer builds install over older ones. Uninstall packages from before 0.3 that were signed with another certificate, and delete any `jsc.exe`, `browser.exe` or `icudt63l.dat` copied to the phone by hand: the installer will not overwrite files it does not own.

Start **WebKit Browser** from the applications menu. **Options → Go to address** opens a page; try `file:///C:/Data/WebKit/test.html`.

## Status

This is an early port. It has been tested in the [EKA2L1](https://github.com/EKA2L1/EKA2L1) emulator with Nokia X7 firmware, where local pages, touch, menus and form controls work. On real phones:

* it has only begun to be tested; reports are welcome
* networking (HTTP and HTTPS through curl and mbedTLS) is built in but not yet tested
* the browser is about 30 MB, so it needs a phone with enough free memory

Not supported: video and audio, WebGL, Web Audio, Web Crypto, service workers, XSLT. Still to come: zoom and a viewport for desktop pages, form controls in the phone's own style, root certificates for HTTPS.

## Development

The port lives on the [`2.32.4-symbian`](https://github.com/yeatse/webkit-symbian/tree/2.32.4-symbian) branch as commits to WebKit itself; this directory holds its build tooling. Building, the layout of the port, CI and the notes from porting it are in [docs/Development.md](docs/Development.md).

WebKit's own files keep their licenses (BSD-2-Clause / LGPL-2.1).
