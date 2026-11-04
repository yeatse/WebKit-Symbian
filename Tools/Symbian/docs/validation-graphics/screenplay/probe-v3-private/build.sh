#!/usr/bin/env bash
set -euo pipefail
probe_source=$(cd "$(dirname "$0")" && pwd)
probe_sdk=${SYMBIAN_SDK_PREFIX:?set SYMBIAN_SDK_PREFIX}
probe_converter=${SYMBIAN_NATIVE_CONVERTER:?set SYMBIAN_NATIVE_CONVERTER}
probe_build=${CONTRACT_BUILD:?set CONTRACT_BUILD to a fresh external directory}
probe_stage=${SCREENPLAY_SDK_STAGE:?set SCREENPLAY_SDK_STAGE to the reviewed external stage}
"$probe_sdk/bin/cmake" -S "$probe_source" -B "$probe_build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$probe_sdk/cmake/symbian-arm.cmake" \
  -DSYMBIAN_SDK_PREFIX="$probe_sdk" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_MAKE_PROGRAM="$probe_sdk/bin/ninja" \
  -DSYMBIAN_NATIVE_CONVERTER="$probe_converter" \
  -DSCREENPLAY_SDK_STAGE="$probe_stage"
"$probe_sdk/bin/ninja" -C "$probe_build" -j1
