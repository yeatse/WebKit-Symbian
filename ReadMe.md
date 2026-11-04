# WebKit for Symbian

A native web browser for Nokia Symbian^3, Anna and Belle phones, including the N8, E7, C7, X7 and 808 PureView. This branch brings WebKit 2.52.6 to their ARM11 processors.

## Features

- Modern HTML, CSS and JavaScript, with ARM11 just-in-time compilation.
- Touch controls, the phone's keyboard and input methods, copy and paste, and file uploads and downloads.
- Accelerated graphics, WebGL 1, audio and video playback, and Web Audio.
- HTTPS, bookmarks, browsing history and persistent website storage.
- Multilingual text and web fonts.

## Download

- **Tagged builds:** choose a version from [Releases](https://github.com/yeatse/WebKit-Symbian/releases).
- **Latest build:** sign in to GitHub, open the latest successful [Symbian build for this branch](https://github.com/yeatse/WebKit-Symbian/actions/workflows/symbian.yml?query=branch%3A2.52.6-symbian), and download the **sis** artifact.

Extract the download and install these packages:

| Package | Purpose |
| --- | --- |
| `symbian-webkit-icu.sis` | Language and text data. Install this first on drive C:. |
| `browser.sis` | WebKit Browser. Launch it from the applications menu. |
| `jsc.sis` | Optional JavaScript command-line shell and example scripts. |

Sign unsigned packages with a certificate accepted by your phone, or use a phone configured for unsigned applications.

[Development](Tools/Symbian/docs/Development.md) · [Project progress](Tools/Symbian/docs/PROGRESS.md) · [Testing](Tools/Symbian/docs/TESTING.md)
