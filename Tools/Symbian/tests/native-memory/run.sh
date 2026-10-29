#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../../.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pthread \
    -I"$here/include" -I"$root/Source/WTF/wtf/symbian" \
    "$here/memory.cpp" "$root/Source/WTF/wtf/symbian/SymbianSystem.cpp" \
    -o "$build/memory"
"$build/memory"
