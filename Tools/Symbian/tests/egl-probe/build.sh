#!/usr/bin/env bash
set -euo pipefail
probe_source=$(cd "$(dirname "$0")" && pwd)
probe_webkit=$(cd "$probe_source/../../../.." && pwd)
probe_work=${SYMBIAN_WEBKIT_WORK:-"$probe_webkit/../symbian-work"}
probe_build=${PROBE_BUILD:-"$probe_work/build/egl-probe"}
probe_sdk=${SYMBIAN_SDK_PREFIX:?Set SYMBIAN_SDK_PREFIX to the SDK root}
probe_converter=${SYMBIAN_NATIVE_CONVERTER:?Set SYMBIAN_NATIVE_CONVERTER to the symbian-native converter}
"$probe_sdk/bin/cmake" -S "$probe_source" -B "$probe_build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$probe_sdk/cmake/symbian-arm.cmake" \
    -DSYMBIAN_SDK_PREFIX="$probe_sdk" -DCMAKE_MAKE_PROGRAM="$probe_sdk/bin/ninja" \
    -DCMAKE_BUILD_TYPE=Release -DSYMBIAN_NATIVE_CONVERTER="$probe_converter"
"$probe_sdk/bin/ninja" -C "$probe_build" -j1
# The firmware driver needs more than the SDK's default 1 MB process heap.
python3 "$probe_webkit/Tools/Symbian/scripts/e32-header.py" \
    "$probe_build/e32/egl_probe.exe" 0x10000 0x1000000 0x40000
python3 "$probe_webkit/Tools/Symbian/scripts/e32info.py" "$probe_build/e32/egl_probe.exe"
sha256sum "$probe_build/e32/egl_probe.exe"
