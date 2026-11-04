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

Extract the download, install `symbian-webkit.sis` and launch WebKit Browser from the applications menu. The package includes the browser, JavaScript command-line shell, language data and example scripts.

Sign an unsigned package with a certificate accepted by your phone, or use a phone configured for unsigned applications.

[Development](Tools/Symbian/docs/Development.md) · [Project progress](Tools/Symbian/docs/PROGRESS.md) · [Testing](Tools/Symbian/docs/TESTING.md)
