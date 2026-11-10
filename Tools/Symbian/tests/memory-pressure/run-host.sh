#!/bin/sh
set -eu
pressure_test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
pressure_repo=$(CDPATH= cd -- "$pressure_test_dir/../../../.." && pwd)
pressure_source="$pressure_repo/Source/WTF/wtf/symbian"
if [ "$#" -ne 1 ] || [ -e "$1" ]; then
    echo "usage: $0 NEW_OUTPUT_DIRECTORY" >&2
    exit 2
fi
mkdir -p -- "$1"
pressure_output=$(CDPATH= cd -- "$1" && pwd)
"${CXX:-c++}" -std=c++20 -O2 -I"$pressure_source" "$pressure_test_dir/policy.cpp" -o "$pressure_output/policy"
"$pressure_output/policy" > "$pressure_output/policy.log"
cat "$pressure_output/policy.log"
"${CXX:-c++}" -std=c++20 -O2 -I"$pressure_test_dir/shim" -I"$pressure_source" \
    "$pressure_test_dir/native-lifecycle.cpp" "$pressure_source/MemoryPressureMonitorSymbianNative.cpp" \
    -o "$pressure_output/native-lifecycle"
"$pressure_output/native-lifecycle" > "$pressure_output/native-lifecycle.log"
cat "$pressure_output/native-lifecycle.log"
