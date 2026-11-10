#!/usr/bin/env bash
set -euo pipefail
test_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_root=$(cd -- "$test_dir/../../../.." && pwd)
test_build=$(mktemp -d)
trap 'rm -rf -- "$test_build"' EXIT
for test_name in tracker gesture; do
    "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
        -I"$source_root/Source/WebKitLegacy/symbian" \
        -I"$source_root/Tools/Symbian/browser" \
        "$test_dir/$test_name.cpp" -o "$test_build/$test_name"
    "$test_build/$test_name"
done
