#!/usr/bin/env bash
set -euo pipefail
probe_source=$(cd "$(dirname "$0")" && pwd)
probe_sdk=${SYMBIAN_SDK_PREFIX:?set SYMBIAN_SDK_PREFIX}
probe_converter=${SYMBIAN_NATIVE_CONVERTER:?set SYMBIAN_NATIVE_CONVERTER}
probe_build=${PROBE_BUILD:?set PROBE_BUILD}
python3 "$probe_source/prepare.py" --build "$probe_build"
"$probe_sdk/bin/cmake" -S "$probe_source" -B "$probe_build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$probe_sdk/cmake/symbian-arm.cmake" \
    -DSYMBIAN_SDK_PREFIX="$probe_sdk" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_MAKE_PROGRAM="$probe_sdk/bin/ninja" \
    -DSYMBIAN_NATIVE_CONVERTER="$probe_converter"
"$probe_sdk/bin/ninja" -C "$probe_build" -j1
python3 "$probe_source/../../scripts/e32-header.py" "$probe_build/e32/idb_copy_probe.exe" 0x20000 0x800000 0x20000
sha256sum "$probe_build/e32/idb_copy_probe.exe"
