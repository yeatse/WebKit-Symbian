#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
. "$source_dir/../../scripts/common.sh"
require_sdk
: "${EKA_SOURCE_DIR:?Set EKA_SOURCE_DIR to the patched EKA2L1 checkout}"
media_work=${NATIVE_MEDIA_WORK:-$WORK/eka-native-media}
converter=${SYMBIAN_NATIVE_CONVERTER:-$SYMBIAN_NATIVE}
test -x "$converter"
python3 "$REPO/scripts/apply-sdk-patches.py" --sdk "$SDK"
# The SDK validates DEF paths beneath the CMake source root. Stage this small
# project in the work directory so generated definitions never dirty WebKit.
mkdir -p "$media_work/source" "$media_work/logs"
"$SDK/bin/cmake" -E copy_if_different "$source_dir/CMakeLists.txt" \
    "$source_dir/native-patch-compat.h" "$media_work/source/"
"$SDK/bin/cmake" -S "$media_work/source" -B "$media_work/build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$SDK/cmake/symbian-arm.cmake" \
    -DSYMBIAN_SDK_PREFIX="$SDK" -DCMAKE_MAKE_PROGRAM="$SDK/bin/ninja" \
    -DCMAKE_BUILD_TYPE=Release -DEKA_SOURCE_DIR="$EKA_SOURCE_DIR" \
    -DSYMBIAN_NATIVE_CONVERTER="$converter" \
    2>&1 | tee "$media_work/logs/configure.log"
"$SDK/bin/ninja" -C "$media_work/build" -j1 \
    mediaclientaudio_general mediaclientvideo_v100 \
    2>&1 | tee "$media_work/logs/build.log"
(
    cd "$media_work/build"
    sha256sum mediaclientaudio_general.dll mediaclientvideo_v100.dll > SHA256SUMS
    cat SHA256SUMS
)
