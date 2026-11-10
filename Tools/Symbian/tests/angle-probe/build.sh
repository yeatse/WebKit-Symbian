#!/usr/bin/env bash
set -euo pipefail
probe_source=$(cd "$(dirname "$0")" && pwd)
probe_webkit=$(cd "$probe_source/../../../.." && pwd)
probe_sdk=${SYMBIAN_SDK_PREFIX:?set SYMBIAN_SDK_PREFIX to the SDK}
probe_angle=${ANGLE_BUILD_DIR:?set ANGLE_BUILD_DIR to the Symbian ANGLE build}
probe_converter=${SYMBIAN_NATIVE_CONVERTER:?set SYMBIAN_NATIVE_CONVERTER to the patched converter}
probe_build=${PROBE_BUILD:-"$probe_angle/angle-probe"}
"$probe_sdk/bin/cmake" -S "$probe_source" -B "$probe_build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$probe_sdk/cmake/symbian-arm.cmake" \
    -DSYMBIAN_SDK_PREFIX="$probe_sdk" -DCMAKE_MAKE_PROGRAM="$probe_sdk/bin/ninja" \
    -DCMAKE_BUILD_TYPE=Release -DSYMBIAN_NATIVE_CONVERTER="$probe_converter" \
    -DWEBKIT_SOURCE_DIR="$probe_webkit" -DANGLE_BUILD_DIR="$probe_angle"
"$probe_sdk/bin/ninja" -C "$probe_build" -j1
python3 "$probe_webkit/Tools/Symbian/scripts/e32-header.py" \
    "$probe_build/e32/angle_probe.exe" 0x10000 0x4000000 0x80000
python3 "$probe_webkit/Tools/Symbian/scripts/e32info.py" "$probe_build/e32/angle_probe.exe"
sha256sum "$probe_build/e32/angle_probe.exe"
