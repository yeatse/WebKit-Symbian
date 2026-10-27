#!/usr/bin/env bash
# Packages the built jsc.exe and browser.exe as SIS installation files in
# $WORK/sis:
#
#   symbian-webkit-icu.sis  ICU data, C:\resource\icu\icudt63l.dat. Both
#                           programs read it, and the installer does not let
#                           two packages own one file; install it first.
#   jsc.sis                 jsc.exe and the test scripts in C:\Data
#   browser.sis             browser.exe with its menu registration and
#                           C:\Data\WebKit\test.html
#
# Packages are unsigned. SIS_CERTIFICATE and SIS_KEY name a PEM certificate
# and RSA key to sign them with instead; updating an installed package then
# needs the certificate it was signed with.
. "$(dirname "$0")/common.sh"
out=$WORK/sis
mkdir -p "$out"
version=${SIS_VERSION:-0.3.0}
vendor=symbian-webkit
icu=$ICU_DIR/install/share/icu/icudt63l.dat
jsc=$E32_BUILD/e32/jsc.exe
browser=$BROWSER_BUILD/e32

sign=()
if [ -n "${SIS_CERTIFICATE:-}" ] || [ -n "${SIS_KEY:-}" ]; then
  sign=(--certificate "${SIS_CERTIFICATE:?SIS_KEY needs SIS_CERTIFICATE}" --key "${SIS_KEY:?SIS_CERTIFICATE needs SIS_KEY}")
fi
# Target devices: Symbian^3 and Anna (S60 5.2), Belle (S60 5.3). Without them
# the phone's installer warns that the application is not compatible.
platforms=(--platform 0x20022E6D=S60ProductID --platform 0x20032DE7=S60ProductID)
make_sis() { python3 "$REPO/scripts/make-sis.py" --vendor "$vendor" --version "$version" "${platforms[@]}" "${sign[@]}" "$@"; }

make_sis --uid 0xE1A5C010 --name "Symbian WebKit ICU data" --output "$out/symbian-webkit-icu.sis" \
  --file "$icu=c:\\resource\\icu\\icudt63l.dat"

if [ -f "$jsc" ]; then
  make_sis --uid 0xE1A5C011 --name "JavaScriptCore shell" --output "$out/jsc.sis" \
    --file "$jsc=!:\\sys\\bin\\jsc.exe" \
    --file "$REPO/tests/es2020.js=c:\\data\\es2020.js" \
    --file "$REPO/tests/intl.js=c:\\data\\intl.js"
fi

if [ -f "$browser/browser.exe" ]; then
  make_sis --uid 0xE1A5C012 --name "WebKit Browser" --output "$out/browser.sis" \
    --file "$browser/browser.exe=!:\\sys\\bin\\browser.exe" \
    --file "$browser/browser_reg.rsc=!:\\private\\10003a3f\\import\\apps\\browser_reg.rsc" \
    --file "$browser/browser_loc.rsc=!:\\resource\\apps\\browser_loc.rsc" \
    --file "$REPO/browser/test.html=c:\\data\\webkit\\test.html"
fi
ls -l "$out"/*.sis
