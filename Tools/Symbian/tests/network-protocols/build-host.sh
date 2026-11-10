#!/usr/bin/env bash
# Positive loopback tests of the curl adapter and nghttp2 memory policy.
# No SDK or emulator runtime is used by this host build.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
symbian=$(cd "$here/../.." && pwd)
work=${1:?Usage: build-host.sh WORK CURL_ARCHIVE NGHTTP2_ARCHIVE}
curl_archive=${2:?curl 7.88.1 archive required}
nghttp2_archive=${3:?nghttp2 1.68.0 archive required}
mkdir -p "$work/src" "$work/install"
tar -xf "$curl_archive" -C "$work/src"
tar -xf "$nghttp2_archive" -C "$work/src"
python3 "$symbian/scripts/patch-curl-network.py" "$work/src/curl-7.88.1"
# Select only the platform network branches in the host copy. Defining
# __SYMBIAN32__ globally would incorrectly select the Open C host ABI.
python3 - "$work/src/curl-7.88.1" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
for name in ('lib/curl_addrinfo.c', 'lib/http2.c', 'lib/dynbuf.h'):
    path = root / name
    source = path.read_text()
    assert '__SYMBIAN32__' in source, name
    path.write_text(source.replace('__SYMBIAN32__', 'CURL_BELLE_HOST_TEST'))
PY
cmake -S "$work/src/nghttp2-1.68.0" -B "$work/nghttp2" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$work/install" \
  -DENABLE_LIB_ONLY=ON -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF \
  -DENABLE_DOC=OFF -DBUILD_TESTING=OFF -DENABLE_FAILMALLOC=OFF
cmake --build "$work/nghttp2" --target install --parallel 1
cmake -S "$work/src/curl-7.88.1" -B "$work/curl" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$work/install" \
  -DCMAKE_C_FLAGS="-DCURL_BELLE_HOST_TEST -I$symbian/shims" \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DBUILD_CURL_EXE=OFF \
  -DENABLE_IPV6=ON -DENABLE_THREADED_RESOLVER=ON -DCURL_DISABLE_SOCKETPAIR=ON \
  -DCURL_USE_OPENSSL=ON -DCURL_USE_MBEDTLS=OFF -DUSE_NGHTTP2=ON \
  -DNGHTTP2_INCLUDE_DIR="$work/install/include" \
  -DNGHTTP2_LIBRARY="$work/install/lib/libnghttp2.a" \
  -DCURL_USE_LIBSSH2=OFF -DUSE_LIBIDN2=OFF -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF \
  -DCURL_DISABLE_LDAP=ON -DCURL_DISABLE_LDAPS=ON
cmake --build "$work/curl" --target install --parallel 1
cc -std=c11 -D_POSIX_C_SOURCE=200809L -DHAVE_CONFIG_H \
  -I"$work/curl/lib" -I"$work/src/curl-7.88.1/lib" -I"$work/install/include" \
  "$here/client.c" "$work/install/lib/libcurl.a" "$work/install/lib/libnghttp2.a" \
  -lssl -lcrypto -lz -lpsl -lpthread -ldl -o "$work/client"
cc -std=c11 -Wall -Wextra -Werror -I"$symbian/shims" \
  "$here/budget.c" -o "$work/budget"
"$work/budget"
