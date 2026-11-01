#!/usr/bin/env bash
set -euo pipefail
contract_source=$(cd "$(dirname "$0")" && pwd)
contract_sdk=${SYMBIAN_SDK_PREFIX:?set SYMBIAN_SDK_PREFIX}
contract_converter=${SYMBIAN_NATIVE_CONVERTER:?set SYMBIAN_NATIVE_CONVERTER}
contract_build=${CONTRACT_BUILD:?set CONTRACT_BUILD outside SDK/EKA/shared source}
"$contract_sdk/bin/cmake" -S "$contract_source" -B "$contract_build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$contract_sdk/cmake/symbian-arm.cmake" \
  -DSYMBIAN_SDK_PREFIX="$contract_sdk" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_MAKE_PROGRAM="$contract_sdk/bin/ninja" \
  -DSYMBIAN_NATIVE_CONVERTER="$contract_converter"
"$contract_sdk/bin/ninja" -C "$contract_build" -j1
rm -f "$contract_build/native-inspection.json"
if [[ -n ${CONTRACT_INSPECTOR:-} ]]; then
  "$CONTRACT_INSPECTOR" image "$contract_build/e32/file_io_contract.exe" \
    > "$contract_build/native-inspection.json"
fi
sha256sum "$contract_build/e32/file_io_contract.exe"
