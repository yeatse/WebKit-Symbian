#!/usr/bin/env bash
set -euo pipefail
download_tests=$(cd "$(dirname "$0")" && pwd)
download_root=$(cd "$download_tests/../../../.." && pwd)
download_build=${DOWNLOAD_TEST_BUILD:?set DOWNLOAD_TEST_BUILD to an external build directory}
mkdir -p "$download_build"
"${CXX:-c++}" -std=c++23 -Wall -Wextra -Werror \
    -I "$download_tests/native-fake" \
    -I "$download_root/Source/WebKitLegacy/symbian" \
    -I "$download_root/Tools/Symbian/browser" \
    "$download_tests/native-file.cpp" \
    "$download_root/Tools/Symbian/browser/DownloadFileNative.cpp" \
    -o "$download_build/native-file-tests"
"$download_build/native-file-tests"
