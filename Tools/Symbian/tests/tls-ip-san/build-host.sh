#!/usr/bin/env bash
set -euo pipefail
test_source=$(cd "$(dirname "$0")" && pwd)
mbedtls_source=${1:?usage: build-host.sh source host-library-directory abseil-include output}
mbedtls_libraries=${2:?missing host-library-directory}
abseil_include=${3:?missing abseil-include}
test_output=${4:?missing output directory}
mkdir -p "$test_output"
test_flags=(-std=c11 -O2 -Wall -Wextra -Werror -DMBEDTLS_STATIC
    '-DMBEDTLS_USER_CONFIG_FILE="symbian_mbedtls/config.h"' -DMBEDTLS_SSL_PROTO_TLS1_3
    -I"$mbedtls_source/include" -I"$mbedtls_source/library" -I"$abseil_include")
"${CC:-cc}" "${test_flags[@]}" "$test_source/probe.c" \
    "$mbedtls_libraries/libmbedtls.a" "$mbedtls_libraries/libmbedx509.a" \
    "$mbedtls_libraries/libmbedcrypto.a" -pthread -o "$test_output/probe"
if [ -f "$mbedtls_source/library/symbian_x509_ip.h" ]; then
    "${CC:-cc}" "${test_flags[@]}" "$test_source/parser.c" \
        "$mbedtls_libraries/libmbedcrypto.a" -pthread -o "$test_output/parser"
fi
