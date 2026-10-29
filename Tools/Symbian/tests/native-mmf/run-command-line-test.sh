#!/usr/bin/env bash
set -euo pipefail
src_dir=$(cd -- "$(dirname -- "$0")" && pwd)
build_dir=$(mktemp -d)
trap 'rm -rf -- "$build_dir"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    "$src_dir/command-line-test.cpp" -o "$build_dir/command-line-test"
"$build_dir/command-line-test"
