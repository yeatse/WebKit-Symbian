#!/usr/bin/env bash
# Builds e32/romprobe.exe and romprobe.sis in $PROBE_BUILD.
set -euo pipefail
probe_source=$(cd "$(dirname "$0")" && pwd)
probe_webkit=$(cd "$probe_source/../../../.." && pwd)
probe_work=${SYMBIAN_WEBKIT_WORK:-"$probe_webkit/../symbian-work"}
probe_build=${PROBE_BUILD:-"$probe_work/build/rom-probe"}
probe_sdk=${SYMBIAN_SDK_PREFIX:?Set SYMBIAN_SDK_PREFIX to the SDK root}
"$probe_sdk/bin/cmake" -S "$probe_source" -B "$probe_build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$probe_sdk/cmake/symbian-arm.cmake" \
    -DSYMBIAN_SDK_PREFIX="$probe_sdk" -DCMAKE_MAKE_PROGRAM="$probe_sdk/bin/ninja" \
    -DCMAKE_BUILD_TYPE=Release
"$probe_sdk/bin/ninja" -C "$probe_build"
# Default heap and stack.
python3 "$probe_webkit/Tools/Symbian/scripts/e32-header.py" \
    "$probe_build/e32/romprobe.exe" 0x10000 0x400000 0x10000
python3 "$probe_webkit/Tools/Symbian/scripts/make-sis.py" --uid 0xE1A5C3B5 \
    --name "ROM probe" --vendor symbian-webkit --version 1.0.0 \
    --platform 0x20022E6D=S60ProductID --platform 0x20032DE7=S60ProductID \
    --file "$probe_build/e32/romprobe.exe=c:\\sys\\bin\\romprobe.exe" \
    --output "$probe_build/romprobe.sis"
sha256sum "$probe_build/e32/romprobe.exe" "$probe_build/romprobe.sis"
