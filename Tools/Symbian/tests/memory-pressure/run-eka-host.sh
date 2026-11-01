#!/bin/sh
set -eu
if [ "$#" -ne 2 ] || [ -e "$2" ]; then
    echo "usage: $0 EKA2L1_SOURCE NEW_OUTPUT_DIRECTORY" >&2
    exit 2
fi
pressure_tests=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
pressure_eka=$(CDPATH= cd -- "$1" && pwd)
mkdir -p -- "$2"
pressure_output=$(CDPATH= cd -- "$2" && pwd)
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror \
    -I"$pressure_tests/eka-shim" -I"$pressure_eka/src/emu/kernel/include" \
    "$pressure_tests/eka-notifier.cpp" "$pressure_eka/src/emu/kernel/src/change_notifier.cpp" \
    -o "$pressure_output/eka-notifier"
"$pressure_output/eka-notifier" > "$pressure_output/eka-notifier.log"
cat "$pressure_output/eka-notifier.log"
