#!/usr/bin/env bash
set -euo pipefail
test_source=$(cd "$(dirname "$0")" && pwd)
curl_source=${1:?usage: run-host.sh /path/to/curl-7.88.1}
test_build=$(mktemp -d)
trap 'rm -rf "$test_build"' EXIT
test_flags=(-std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror
    -I"$curl_source/lib" -I"$curl_source/include" -I"$test_source"
    -include "$test_source/symbian-abi.h")
for variant in old corrected; do
    variant_flags=()
    if [ "$variant" = corrected ]; then
        variant_flags+=('-DCURL_SA_FAMILY_T=unsigned short')
    fi
    "${CC:-cc}" "${test_flags[@]}" "${variant_flags[@]}" \
        "$curl_source/lib/curl_addrinfo.c" "$test_source/probe.c" \
        -o "$test_build/$variant"
done
set +e
"$test_build/old"
old_status=$?
set -e
if [ "$old_status" != 1 ]; then
    echo "FAIL: old curl family cast must reject all numeric IPv4 cases" >&2
    exit 1
fi
"$test_build/corrected"
echo 'PASS: unchanged curl address builders reproduce old truncation and preserve corrected families'
