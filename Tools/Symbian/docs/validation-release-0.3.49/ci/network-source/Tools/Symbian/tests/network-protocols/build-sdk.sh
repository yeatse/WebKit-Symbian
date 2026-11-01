#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
symbian=$(cd "$here/../.." && pwd)
out=${1:?Usage: build-sdk.sh OUTPUT NETWORK_DEPS OTHER_DEPS ICU}
network=${2:?Network dependency install prefix required}
deps=${3:?Other dependency install prefix required}
icu=${4:?ICU install prefix required}
sdk=${SYMBIAN_SDK_PREFIX:?SDK required}
mkdir -p "$out"
"$symbian/toolchain/sym-cc" -std=c11 -DCURL_STATICLIB -DNGHTTP2_STATICLIB \
  -mfloat-abi=softfp -mfpu=vfpv2 -I"$network/include" \
  -c "$here/sdk-link.c" -o "$out/sdk-link.o"
"$symbian/toolchain/sym-c++" -mfloat-abi=softfp -mfpu=vfpv2 -Wl,--gc-sections \
  "$out/sdk-link.o" -o "$out/sdk-link.elf" \
  "$sdk/proxies/esock/esock.dso" "$sdk/proxies/insock/insock.dso" \
  "$network/lib/libcurl.a" "$network/lib/libnghttp2.a" "$deps/lib/libmbedtls_fs.a" \
  "$sdk/lib/armv6/mbedtls/lib/libmbedtls.a" "$sdk/lib/armv6/mbedtls/lib/libmbedx509.a" \
  "$sdk/lib/armv6/mbedtls/lib/libmbedcrypto.a" "$deps/lib/libpsl.a" \
  "$icu/lib/libicuuc.a" "$icu/lib/libicudata.a" "$sdk/lib/armv6/libsymbian_portable_zlib.a"
file "$out/sdk-link.elf"
