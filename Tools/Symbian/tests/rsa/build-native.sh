#!/usr/bin/env bash
set -euo pipefail
probe_source=$(cd "$(dirname "$0")" && pwd)
probe_checkout=$(cd "$probe_source/../../../.." && pwd)
probe_sdk=${SYMBIAN_SDK_PREFIX:?set SYMBIAN_SDK_PREFIX}
probe_converter=${SYMBIAN_NATIVE_CONVERTER:?set SYMBIAN_NATIVE_CONVERTER}
probe_build=${PROBE_BUILD:?set PROBE_BUILD}
probe_webkit=${WEBKIT_BUILD:?set WEBKIT_BUILD}
mkdir -p "$probe_build"
python3 "$probe_source/prepare-vectors.py" --work "$probe_build"
"$probe_sdk/bin/cmake" -S "$probe_source" -B "$probe_build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$probe_sdk/cmake/symbian-arm.cmake" \
    -DSYMBIAN_SDK_PREFIX="$probe_sdk" -DCMAKE_MAKE_PROGRAM="$probe_sdk/bin/ninja" \
    -DCMAKE_BUILD_TYPE=Release -DSYMBIAN_NATIVE_CONVERTER="$probe_converter" \
    -DWEBKIT_BUILD="$probe_webkit"
"$probe_sdk/bin/ninja" -C "$probe_build" -j1
python3 "$probe_checkout/Tools/Symbian/scripts/e32-header.py" \
    "$probe_build/e32/rsa_probe.exe" 0x20000 0x2000000 0x80000
sha256sum "$probe_build/e32/rsa_probe.exe"
