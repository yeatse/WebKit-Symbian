#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
browser=$(cd "$here/../../browser" && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -I"$browser" "$here/settings.cpp" "$browser/BrowserSettings.cpp" -o "$build/settings"
mkdir "$build/data"
"$build/settings" "$build/data"
