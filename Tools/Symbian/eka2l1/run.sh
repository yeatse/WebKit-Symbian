#!/usr/bin/env bash
# Runs the E32 jsc.exe in EKA2L1 and prints what it wrote.
#
#   eka2l1/run.sh [JSC ARGUMENTS...]      e.g. eka2l1/run.sh C:/Data/es2020.js
#
# Copies jsc.exe, the ICU data file and tests/*.js onto the emulated
# drive C:, starts the emulator on EKA2L1_DEVICE (under Xvfb when there is no
# display) and prints C:\Data\jsc.log, where jsc on Symbian sends stdout and
# stderr. The device must already be installed in EKA2L1.
#
#   EKA2L1          path to the eka2l1_qt executable (required)
#   EKA2L1_DATA     the emulator's data directory (~/.local/share/EKA2L1/data)
#   EKA2L1_DEVICE   firmware code of the device to run (RM-707, Nokia X7)
#   TIMEOUT         seconds before the emulator is stopped (600)
. "$(dirname "$0")/../scripts/common.sh"
: "${EKA2L1:?set EKA2L1 to the eka2l1_qt executable}"
data=${EKA2L1_DATA:-$HOME/.local/share/EKA2L1/data}
device=${EKA2L1_DEVICE:-RM-707}
c=$data/drives/c
mkdir -p "$c/sys/bin" "$c/Data" "$c/resource/icu"
cp "$E32_BUILD/e32/jsc.exe" "$c/sys/bin/"
[ -f "$ICU_DIR/install/share/icu/icudt63l.dat" ] && cp "$ICU_DIR/install/share/icu/icudt63l.dat" "$c/resource/icu/"
cp "$REPO"/tests/*.js "$c/Data/"
rm -f "$c/Data/jsc.log"

# EKA2L1 treats an argument string that starts with "--" as one of its own
# options; jsc skips the leading space.
args=" $*"
run=(timeout "${TIMEOUT:-600}")
[ -z "${DISPLAY:-}" ] && run+=(xvfb-run -a)
log=$WORK/eka2l1-run.log
(cd "$(dirname "$EKA2L1")" && "${run[@]}" "$EKA2L1" --device "$device" --run 'C:\sys\bin\jsc.exe' "$args") > "$log" 2>&1
grep -a -E "Access violation|terminated|killed with" "$log" | sed 's/^.*\]: //' >&2
cat "$c/Data/jsc.log" 2>/dev/null
