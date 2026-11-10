#!/usr/bin/env bash
set -euo pipefail
src_dir=$(cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(cd -- "$src_dir/../../../.." && pwd)
build_dir=$(mktemp -d)
trap 'rm -rf -- "$build_dir"' EXIT
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pthread \
    -I "$repo_dir/Source/WebCore/platform/graphics/egl" \
    "$src_dir/resource-state.cpp" -o "$build_dir/resource-state"
"$build_dir/resource-state"
