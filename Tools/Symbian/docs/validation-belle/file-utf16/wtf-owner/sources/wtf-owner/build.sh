#!/usr/bin/env bash
set -euo pipefail
contract_source=$(cd "$(dirname "$0")" && pwd)
contract_sdk=${SYMBIAN_SDK_PREFIX:?set SYMBIAN_SDK_PREFIX}
contract_converter=${SYMBIAN_NATIVE_CONVERTER:?set SYMBIAN_NATIVE_CONVERTER}
contract_build=${CONTRACT_BUILD:-"$contract_source/build"}
contract_webkit_build=${WEBKIT_BUILD_DIR:-/workspace/symbian-graphics-work/build/webcore}
contract_mode=${1:---compile-only}
case "$contract_mode" in
  --compile-only) ;;
  --link)
    : "${CONTRACT_WTF_SHA256:?Root must supply the new completed libWTF.a SHA256 before linking}"
    contract_actual=$(sha256sum "$contract_webkit_build/lib/libWTF.a")
    contract_actual=${contract_actual%% *}
    [[ "$contract_actual" == "$CONTRACT_WTF_SHA256" ]] || { echo 'completed libWTF.a pin differs' >&2; exit 2; }
    ;;
  *) echo 'usage: build.sh [--compile-only|--link]' >&2; exit 2 ;;
esac
"$contract_sdk/bin/cmake" -S "$contract_source" -B "$contract_build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$contract_sdk/cmake/symbian-arm.cmake" \
  -DSYMBIAN_SDK_PREFIX="$contract_sdk" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_MAKE_PROGRAM="$contract_sdk/bin/ninja" \
  -DSYMBIAN_NATIVE_CONVERTER="$contract_converter" \
  -DWEBKIT_BUILD_DIR="$contract_webkit_build"
if [[ "$contract_mode" == --compile-only ]]; then
  "$contract_sdk/bin/ninja" -C "$contract_build" -j1 CMakeFiles/wtf_file_owner_contract.dir/worker.cpp.obj
else
  "$contract_sdk/bin/ninja" -C "$contract_build" -j1
  contract_actual=$(sha256sum "$contract_webkit_build/lib/libWTF.a")
  contract_actual=${contract_actual%% *}
  [[ "$contract_actual" == "$CONTRACT_WTF_SHA256" ]] || { echo 'libWTF.a changed during link; reject artifact' >&2; exit 2; }
  sha256sum "$contract_build/e32/wtf_file_owner_contract.exe"
fi
