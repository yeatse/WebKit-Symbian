#!/usr/bin/env bash
set -euo pipefail
download_tests=$(cd "$(dirname "$0")" && pwd)
download_root=$(cd "$download_tests/../../../.." && pwd)
download_build=${DOWNLOAD_TEST_BUILD:?set DOWNLOAD_TEST_BUILD to an external build directory}
mkdir -p "$download_build"
"${CXX:-c++}" -std=c++23 -Wall -Wextra -Werror \
    -I "$download_root/Source/WebKitLegacy/symbian" \
    "$download_tests/filename.cpp" -o "$download_build/filename-tests"
"$download_build/filename-tests"
