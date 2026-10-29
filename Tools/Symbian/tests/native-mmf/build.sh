#!/usr/bin/env bash
set -euo pipefail
probe_source=$(cd "$(dirname "$0")" && pwd)
probe_build=${PROBE_BUILD:-"$probe_source/../native-probe-build"}
probe_sdk=${SYMBIAN_SDK_PREFIX:-/workspace/symbian-sdk}
probe_webkit=${WEBKIT_SOURCE_DIR:-/workspace/WebKit-Symbian-graphics}
probe_converter=${SYMBIAN_NATIVE_CONVERTER:-/workspace/symbian-work/symbian-native/cpp/symbian/tools/symbian-native}
"$probe_sdk/bin/cmake" -S "$probe_source" -B "$probe_build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$probe_sdk/cmake/symbian-arm.cmake" \
    -DSYMBIAN_SDK_PREFIX="$probe_sdk" -DCMAKE_MAKE_PROGRAM="$probe_sdk/bin/ninja" \
    -DCMAKE_BUILD_TYPE=Release -DSYMBIAN_NATIVE_CONVERTER="$probe_converter"
"$probe_sdk/bin/ninja" -C "$probe_build" -j1
python3 "$probe_webkit/Tools/Symbian/scripts/e32-header.py" \
    "$probe_build/e32/mmf_probe.exe" 0x10000 0x1000000 0x40000
python3 "$probe_webkit/Tools/Symbian/scripts/e32info.py" "$probe_build/e32/mmf_probe.exe"
sha256sum "$probe_build/e32/mmf_probe.exe"
