#!/usr/bin/env bash
# Runs browser.exe in EKA2L1 and saves a screenshot after a delay.
#
#   eka2l1/run-browser.sh BROWSER_EXE [URL]
#
# browser_reg.rsc and browser_loc.rsc from the same directory are installed
# as AppArc's registration and caption.
#
#   EKA2L1          path to the eka2l1_qt executable (required)
#   EKA2L1_DATA     the emulator's data directory (~/.local/share/EKA2L1/data)
#   EKA2L1_DEVICE   firmware code of the device to run (RM-707, Nokia X7)
#   WAIT            seconds before the screenshot (60)
#   SCREENSHOT      where to save it ($WORK/browser.png)
. "$(dirname "$0")/../scripts/common.sh"
: "${EKA2L1:?set EKA2L1 to the eka2l1_qt executable}"
exe=${1:?usage: run-browser.sh BROWSER_EXE [URL]}
data=${EKA2L1_DATA:-$HOME/.local/share/EKA2L1/data}
c=$data/drives/c
mkdir -p "$c/sys/bin" "$c/Data/WebKit" "$c/Private/10003a3f/import/apps" "$c/resource/apps"
cp "$exe" "$c/sys/bin/browser.exe"
cp "$(dirname "$exe")/browser_reg.rsc" "$c/Private/10003a3f/import/apps/"
cp "$(dirname "$exe")/browser_loc.rsc" "$c/resource/apps/"
log=$WORK/eka2l1-browser.log
shot=${SCREENSHOT:-$WORK/browser.png}
display=:${DISPLAY_NUMBER:-91}
Xvfb "$display" -screen 0 1280x900x24 >/dev/null 2>&1 &
xvfb=$!
trap 'kill $emulator $xvfb 2>/dev/null' EXIT
sleep 2
(cd "$(dirname "$EKA2L1")" && DISPLAY=$display exec "$EKA2L1" --device "${EKA2L1_DEVICE:-RM-707}" \
  --run 'C:\sys\bin\browser.exe' " ${2:-}") > "$log" 2>&1 &
emulator=$!
sleep "${WAIT:-60}"
DISPLAY=$display import -window root "$shot"
grep -a -E "Access violation|panicked|terminated|killed with category: [^N]" "$log" | sed 's/^.*\]: //'
echo "screenshot: $shot"
