#!/usr/bin/env bash
# Package WebKit Browser, JavaScriptCore and ICU as symbian-webkit.sis.
# SIS_CERTIFICATE and SIS_KEY select a PEM certificate and RSA signing key.
. "$(dirname "$0")/common.sh"
out=$WORK/sis
mkdir -p "$out"
version=${SIS_VERSION:-0.3.0}
icu=$ICU_DIR/install/share/icu/icudt70l.dat
browser=$BROWSER_BUILD/e32
# jsc.exe and browser.exe as split-e32.py splits them, with their DLLs.
jsc_split=$E32_BUILD/split
browser_split=$BROWSER_BUILD/split
# Mozilla's trusted roots, as curl's maintainers extract them.
cacert=$WORK/downloads/cacert-2026-09-25.pem
cacert_sha256=a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505

sign=()
if [ -n "${SIS_CERTIFICATE:-}" ] || [ -n "${SIS_KEY:-}" ]; then
  sign=(--certificate "${SIS_CERTIFICATE:?SIS_KEY needs SIS_CERTIFICATE}" --key "${SIS_KEY:?SIS_CERTIFICATE needs SIS_KEY}")
fi
# Symbian^3/Anna and Belle target device IDs.
platforms=(--platform 0x20022E6D=S60ProductID --platform 0x20032DE7=S60ProductID)
files=()
case "$ICU_DATA" in
  file) files+=(--file "$icu=!:\\resource\\icu\\icudt70l.dat") ;;
  static) ;;
  *) echo "ICU_DATA must be file or static" >&2; exit 1 ;;
esac

if [ ! -f "$cacert" ]; then
  mkdir -p "$(dirname "$cacert")"
  curl -sSfL -o "$cacert.tmp" "https://curl.se/ca/$(basename "$cacert")"
  echo "$cacert_sha256  $cacert.tmp" | sha256sum -c --quiet
  mv "$cacert.tmp" "$cacert"
fi

# Frame-rate benchmark pages (tests/benchmarks), opened from
# file:///C:/Data/WebKit/bench/index.html.
bench=()
for page in "$REPO"/tests/benchmarks/*.html "$REPO"/tests/benchmarks/*.js; do
  bench+=(--file "$page=c:\\data\\webkit\\bench\\$(basename "$page")")
done

# The user picks the drive ("!:"). E: is not demand paged, so the loader
# reads a whole image in and rejects one with more than about 12 MiB of code
# with KErrNoMemory; split-e32.py keeps every image below that. WTF finds the
# ICU data, CA roots and fonts.conf on the executable's drive. Test scripts
# and pages stay on C:, where the documentation opens them.
images=()
for image in "$jsc_split"/*.exe "$jsc_split"/*.dll "$browser_split"/*.exe "$browser_split"/*.dll; do
  [ -f "$image" ] || { echo "missing split images in $(dirname "$image")" >&2; exit 1; }
  images+=(--file "$image=!:\\sys\\bin\\$(basename "$image")")
done
python3 "$REPO/scripts/make-sis.py" \
  --uid 0xE1A5C012 --name "WebKit for Symbian" --vendor symbian-webkit \
  --version "$version" --output "$out/symbian-webkit.sis" \
  "${platforms[@]}" "${sign[@]}" "${files[@]}" \
  "${images[@]}" \
  --file "$REPO/tests/es2020.js=c:\\data\\es2020.js" \
  --file "$REPO/tests/intl.js=c:\\data\\intl.js" \
  --file "$REPO/tests/basic.js=c:\\data\\basic.js" \
  --file "$REPO/tests/resizable-arraybuffer.js=c:\\data\\resizable-arraybuffer.js" \
  --file "$browser/browser_reg.rsc=!:\\private\\10003a3f\\import\\apps\\browser_reg.rsc" \
  --file "$browser/browser_loc.rsc=!:\\resource\\apps\\browser_loc.rsc" \
  --file "$browser/browser_ui.rsc=!:\\resource\\apps\\browser_ui.rsc" \
  --file "$REPO/browser/test.html=c:\\data\\webkit\\test.html" \
  "${bench[@]}" \
  --file "$cacert=!:\\resource\\webkit\\cacert.pem" \
  --file "$REPO/resources/fonts.conf=!:\\resource\\fontconfig\\fonts.conf"
ls -l "$out/symbian-webkit.sis"
