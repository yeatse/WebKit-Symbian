#!/usr/bin/env bash
set -euo pipefail
probe_eka=${1:?usage: check-precision-loader.sh /path/to/patched/EKA2L1}
probe_source=$(cd "$(dirname "$0")" && pwd)
probe_temp=$(mktemp -d)
trap 'rm -rf "$probe_temp"' EXIT
"${HOST_CC:-cc}" -std=c11 -I"$probe_eka/src/external/glad/include" \
    "$probe_eka/src/external/glad/src/glad.c" "$probe_source/precision-loader.c" \
    -ldl -o "$probe_temp/precision-loader"
"$probe_temp/precision-loader"
