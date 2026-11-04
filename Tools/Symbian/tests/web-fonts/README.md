# Web fonts

Checks WOFF2 and TrueType loading, font metrics and rendered pixels. Build the
browser with WOFF2 enabled and install its fontconfig configuration.

```sh
python3 -m http.server 8803 --directory Tools/Symbian/tests/web-fonts
```

Open `http://localhost:8803/index.html` in the browser. The page finishes with
`WEBFONT RESULT 7/7 PASS` when font loading and rendering checks pass.
Open `http://localhost:8803/system-fonts.html` for system-font discovery,
monospace metrics and character fallback; it reports `SYSTEMFONT RESULT 7/7 PASS`.

The Ahem 1.50 source is `Source/ThirdParty/skia/resources/fonts/ahem.ttf`. Its
embedded notice places it in the public domain, with CC0 for jurisdictions
without public-domain ownership; see <http://dev.w3.org/CSS/fonts/ahem/COPYING>.
The notice is embedded in both valid font fixtures. `fonts.json` records fixture
sources and generation details.
