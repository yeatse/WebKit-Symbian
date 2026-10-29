#!/usr/bin/env bash
set -euo pipefail
probe_source=$(cd "$(dirname "$0")" && pwd)
probe_checkout=$(cd "$probe_source/../../../.." && pwd)
probe_sdk=${SYMBIAN_SDK_PREFIX:?set SYMBIAN_SDK_PREFIX}
probe_converter=${SYMBIAN_NATIVE_CONVERTER:?set SYMBIAN_NATIVE_CONVERTER}
probe_build=${PROBE_BUILD:?set PROBE_BUILD}
mkdir -p "$probe_build"
probe_extract_args=()
if [[ -n ${PROBE_POOL_SOURCE:-} ]]; then
    probe_extract_args+=(--pool-source "$PROBE_POOL_SOURCE")
fi
python3 "$probe_source/extract.py" "$probe_checkout" "$probe_build" "${probe_extract_args[@]}"
"$probe_sdk/bin/cmake" -S "$probe_source" -B "$probe_build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$probe_sdk/cmake/symbian-arm.cmake" \
    -DSYMBIAN_SDK_PREFIX="$probe_sdk" -DCMAKE_MAKE_PROGRAM="$probe_sdk/bin/ninja" \
    -DCMAKE_BUILD_TYPE=Release -DSYMBIAN_NATIVE_CONVERTER="$probe_converter" \
    -DWEBKIT_SOURCE_DIR="$probe_checkout"
"$probe_sdk/bin/ninja" -C "$probe_build" -j1
python3 "$probe_checkout/Tools/Symbian/scripts/e32-header.py" \
    "$probe_build/e32/aligned_pressure.exe" 0x20000 0x4000000 0x80000
sha256sum "$probe_build/e32/aligned_pressure.exe"
