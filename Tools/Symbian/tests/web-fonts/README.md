# Font configuration and WOFF2 regression

`build-deps.sh woff2 fontconfig` builds the pinned Brotli 1.2.0 and WOFF2
1.0.2 decoder/common static libraries and the patched fontconfig 2.13.94.
The SDK FreeType archive is unchanged. WebCore's existing `WOFFFileFormat.cpp`
converts WOFF2 to sfnt before FreeType sees it. The browser's `USE_WOFF2` option
must be ON; pass `-DUSE_WOFF2=ON` when reusing a build configured before this
feature was enabled by default.

`resources/fonts.conf` is installed as `C:/resource/fontconfig/fonts.conf` by
the browser SIS. It searches ROM and C: font directories, with a writable
cache on C:, and needs no HOME directory, host paths or external conf.d files.
Generic aliases prefer actual serif/monospace families when present; the
available fonts vary between devices. No phone fonts are redistributed.

Serve this directory unchanged over HTTP(S), or copy all five fixture files
(`index.html` and the four fonts) to `C:/Data/WebKit/web-fonts/` and open
`file:///C:/Data/WebKit/web-fonts/index.html`. Require a fresh
`WEBFONT RESULT 7/7 PASS`, normal browser exit and no guest panic. The page
checks CSS WOFF2 loading, exact em-square metrics, rendered pixels, existing
TTF loading, rejection of both malformed fonts, and a subsequent valid load.
A 15-second timeout fails rather than waiting indefinitely.

The Ahem 1.50 source is the repository's
`Source/ThirdParty/skia/resources/fonts/ahem.ttf`. Its embedded notice places
it in the public domain, with CC0 for jurisdictions without public-domain
ownership; see <http://dev.w3.org/CSS/fonts/ahem/COPYING>. The notice remains
inside both valid fixtures. `fonts.json` records source/output hashes and the
fontTools/Brotli encoder versions. To regenerate `Ahem.woff2`, load `Ahem.ttf`
with `fontTools.ttLib.TTFont(..., recalcTimestamp=False)`, set `flavor='woff2'`
and save. `truncated.woff2` is the first 48 bytes; `invalid-flavor.woff2`
replaces bytes 4–7 with `BAD!`. The latter can be decompressed but must fail
font-engine validation; decoder success alone is not font acceptance.

Host verification uses production library sources, not a replacement decoder:

1. Build `Tools/Symbian/cmake/woff2` without a cross toolchain, setting
   `BROTLI_SOURCE_DIR` and `WOFF2_SOURCE_DIR` to the hash-verified releases in
   `deps/src`, and a scratch `CMAKE_INSTALL_PREFIX`. Compile `decoder-test.cpp`
   against its four installed archives plus host FreeType. Run it with this
   fixture directory as its sole argument; require seven checks and zero
   failures.
2. Extract the verified fontconfig archive to a scratch directory and apply
   `scripts/patch-fontconfig-paths.py` there. Configure a static host build
   with `CFLAGS='-O1 -D__SYMBIAN32__'`, `--enable-libxml2 --disable-shared
   --disable-docs --disable-nls`, and the same C:/Z: paths used by
   `build-deps.sh`. Build `src`, then compile `fontconfig-path-test.c` against
   that archive and host FreeType/libxml2. In a scratch working directory,
   create `D:/overrides/override.conf` containing `<fontconfig/>`; run the test
   with the absolute repository `resources/fonts.conf` path. Require 13
   checks and zero failures. This exercises the actual Symbian path branches,
   including backslashes, a semicolon-separated FONTCONFIG_PATH, absolute
   drive lookup and an absolute `FONTCONFIG_FILE` through `FcConfigFilename`.

Host checks establish decode/path behavior. They do not establish guest
font discovery, rendering or browser integration; those require the page and
installed configuration on the candidate browser.

`system-fonts.html` is a separate seven-check system-font fixture; it does not
replace the WOFF2 checks. It tests real English pixels, equal monospace `i`/`W`
advances with distinct visible glyphs, Chinese `中` and `国` through Arial
fallback, and distinct serif/sans pixels. Both Chinese alpha masks must be
nonempty, differ from each other, and differ from unsupported U+10FFFF. The
RM-707 inventory records that Arial lacks both characters and the ROM's
MHeiM-C-GB18030-S60 includes them; Times New Roman and Nokia Sans S60 are also
present. The serif/sans distinction is an expectation for this known fixture,
not a promise that every phone has both families.

Run it twice in separate browser launches with distinct `?run=` tokens and the
same cache/configuration. Each launch must report a fresh `SYSTEMFONT RESULT
7/7 PASS`, normal exit and no panic. `SYSTEMFONT DETAILS` records each measured
advance, ink count and alpha-mask checksum. Keep the host filesystem log to
confirm that the second launch reads the persisted C: fontconfig cache; repeated
PASS alone does not establish cache reuse. Never copy the proprietary system
fonts into this repository or the test directory.
