#!/usr/bin/env bash
# Links the jsc shell through the SDK image flow and converts it to E32.
. "$(dirname "$0")/common.sh"
require_sdk
"$SDK/bin/cmake" -S "$REPO/e32" -B "$E32_BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$SDK/cmake/symbian-arm.cmake" -DSYMBIAN_SDK_PREFIX="$SDK" \
  -DCMAKE_MAKE_PROGRAM="$SDK/bin/ninja" -DCMAKE_BUILD_TYPE=Release \
  -DSYMBIAN_NATIVE_CONVERTER="$SYMBIAN_NATIVE" \
  -DWEBKIT_BUILD_DIR="$WEBKIT_BUILD" -DICU_LIBRARY_DIR="$ICU_DIR/install/lib" \
  > "$WORK/e32-configure.log"
"$SDK/bin/ninja" -j"$JOBS" -C "$E32_BUILD" > "$WORK/e32-build.log"
python3 "$REPO/scripts/e32-header.py" "$E32_BUILD/e32/jsc.exe" \
  "${JSC_HEAP_MIN:-0x100000}" "${JSC_HEAP_MAX:-0x8000000}" "${JSC_STACK:-0x100000}" --fpu=vfpv2
python3 "$REPO/scripts/e32info.py" "$E32_BUILD/e32/jsc.exe"
