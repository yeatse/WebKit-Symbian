#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../../.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$here/include" "$here/filesystem.cpp" \
    "$root/Source/WTF/wtf/symbian/FileSystemSymbian.cpp" -Wl,--gc-sections \
    -Wl,--wrap=read,--wrap=write,--wrap=rename,--wrap=lstat,--wrap=access,--wrap=getcwd,--wrap=opendir,--wrap=readdir \
    -o "$build/filesystem"
mkdir "$build/C:" "$build/E:"
(cd "$build" && ./filesystem)
