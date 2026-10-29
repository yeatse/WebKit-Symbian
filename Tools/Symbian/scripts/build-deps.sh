#!/usr/bin/env bash
# Static ARMv6 builds of the libraries WebCore needs beyond the SDK's own
# (FreeType, libpng, libjpeg, zlib, mbedTLS): pixman, cairo, HarfBuzz,
# libxml2, libxslt, SQLite, fontconfig, libpsl, libwebp and curl, plus the mbedTLS
# file functions the SDK leaves out (shims/mbedtls-fs.c). Installed into
# $DEPS_DIR/install.
#
#   scripts/build-deps.sh [NAME...]     default: all, in dependency order
. "$(dirname "$0")/common.sh"
require_sdk
src=$DEPS_DIR/src
P=$DEPS_DIR/install
mkdir -p "$src" "$P/lib" "$P/include"

# VFPv2 with the soft-float calling convention, as for JavaScriptCore. Current
# Clang makes some old-C diagnostics errors by default; these libraries predate
# that. Open C's libm macro aliases are off, as the SDK does for C++: they turn
# the SDK's own ldexpl declaration into a conflicting scalbn. -fno-builtin (from
# the SDK flags) would turn alloca into a call to a function Open C lacks.
cflags="-D__LIBM_ALIASES_H__ -Dalloca=__builtin_alloca $OPTIMIZE -mfloat-abi=softfp -mfpu=vfpv2 -Wno-error=incompatible-function-pointer-types -Wno-error=int-conversion -Wno-error=implicit-function-declaration -Wno-error=incompatible-pointer-types -Wno-error=cast-function-type-strict"
sdk_portable=$SDK/include/portable
sdk_lib=$SDK/lib/armv6
# What code using the SDK's mbedTLS must be compiled with (shims/mbedtls-config.h).
tls_flags="-include $REPO/shims/mbedtls-config.h -isystem $SDK/include/abseil -I$REPO/shims"
export CC=$REPO/toolchain/sym-cc CXX=$REPO/toolchain/sym-c++
export AR=$SDK/bin/llvm-ar RANLIB=$SDK/bin/llvm-ranlib
export PKG_CONFIG_PATH=$P/lib/pkgconfig PKG_CONFIG_LIBDIR=$P/lib/pkgconfig

fetch() {  # fetch URL DIR
  local archive=$src/$(basename "$1")
  [ -f "$archive" ] || curl -sSfL -o "$archive" "$1"
  if [ ! -d "$src/$2" ]; then
    case $archive in
      *.zip) (cd "$src" && unzip -q "$archive") ;;
      *) tar -xf "$archive" -C "$src" ;;
    esac
  fi
}

# The SDK's FreeType, libpng and zlib, described for configure scripts.
sdk_pkgconfig() {
  mkdir -p "$P/lib/pkgconfig"
  cat > "$P/lib/pkgconfig/freetype2.pc" <<EOF
Name: FreeType 2
Description: SDK FreeType
Version: 24.1.18
Cflags: -I$sdk_portable/freetype
Libs: -L$sdk_lib -lsymbian_portable_freetype
EOF
  cat > "$P/lib/pkgconfig/libpng.pc" <<EOF
Name: libpng
Description: SDK libpng
Version: 1.6.53
Cflags: -I$sdk_portable/png
Libs: -L$sdk_lib -lsymbian_portable_png -lsymbian_portable_zlib
EOF
  cat > "$P/lib/pkgconfig/zlib.pc" <<EOF
Name: zlib
Description: SDK zlib
Version: 1.3
Cflags: -I$sdk_portable/zlib
Libs: -L$sdk_lib -lsymbian_portable_zlib
EOF
  # build-icu.sh installs only ICU's libraries and headers.
  local icu=$ICU_DIR/install
  cat > "$P/lib/pkgconfig/icu-uc.pc" <<EOF
Name: icu-uc
Description: ICU common library (build-icu.sh)
Version: 63.2
Cflags: -I$icu/include
Libs: -L$icu/lib -licuuc -licudata
EOF
  cat > "$P/lib/pkgconfig/icu-i18n.pc" <<EOF
Name: icu-i18n
Description: ICU internationalization library (build-icu.sh)
Version: 63.2
Requires: icu-uc
Cflags: -I$icu/include
Libs: -L$icu/lib -licui18n
EOF
}

autotools() {  # autotools DIR SUBDIR [CONFIGURE OPTIONS...]: builds and installs SUBDIR only
  local dir=$src/$1 sub=$2; shift 2
  (cd "$dir" && CFLAGS="$cflags" CXXFLAGS="$cflags" ./configure --host=arm-linux-gnueabi \
     --prefix="$P" --enable-static --disable-shared "$@" \
   && make -C "$sub" -j"$JOBS" && make -C "$sub" install && make install-pkgconfigDATA)
}

cmake_build() {  # cmake_build DIR [CMAKE OPTIONS...]
  local dir=$src/$1; shift
  cmake -S "$dir" -B "$dir/build-symbian" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$REPO/toolchain/symbian-armv6.cmake" \
    -DCMAKE_C_FLAGS="$cflags" -DCMAKE_CXX_FLAGS="$cflags" \
    -DCMAKE_INSTALL_PREFIX="$P" -DCMAKE_PREFIX_PATH="$P" \
    -DCMAKE_FIND_ROOT_PATH="$P;$ICU_DIR/install" -DBUILD_SHARED_LIBS=OFF "$@"
  ninja -C "$dir/build-symbian" -j"$JOBS" install
}

build_pixman() {
  fetch https://cairographics.org/releases/pixman-0.40.0.tar.gz pixman-0.40.0
  # Plain C: the ARMv6 SIMD paths are GNU as syntax. No __thread: Symbian
  # has no ELF TLS, so pixman keeps its per-thread cache in a pthread key.
  ac_cv_tls=none autotools pixman-0.40.0 pixman --disable-arm-simd --disable-arm-neon --disable-arm-iwmmxt \
    --disable-gtk --disable-libpng --disable-openmp
}

build_sqlite() {
  fetch https://www.sqlite.org/2021/sqlite-amalgamation-3350500.zip sqlite-amalgamation-3350500
  local dir=$src/sqlite-amalgamation-3350500
  # Open C has no fchown, which SQLite uses only to keep a root process's
  # files owned by their original user. No mmap or extension loading.
  cat > "$dir/symbian-fchown.h" <<'EOF'
#include <sys/types.h>
static int sqliteNoFchown(int fd, uid_t uid, gid_t gid) { (void)fd; (void)uid; (void)gid; return 0; }
#define fchown sqliteNoFchown
EOF
  # Paths with a drive (C:\Data\WebKit\cookies.db) are absolute; the Unix
  # VFS put the current directory in front of them.
  grep -q 'Symbian: a drive' "$dir/sqlite3.c" || perl -0pi - "$dir/sqlite3.c" <<'EOF'
s{(static int mkFullPathname\(.*?int i, j;\n)}{$1  if( zPath[0] && zPath[1]==':' ){ /* Symbian: a drive path is absolute. */
    if( nPath+1>nOut ) return SQLITE_CANTOPEN_BKPT;
    memcpy(zOut, zPath, nPath+1);
    return SQLITE_OK;
  }
}s;
EOF
  grep -q 'Symbian: a drive' "$dir/sqlite3.c"
  "$CC" $cflags -include "$dir/symbian-fchown.h" -DSQLITE_OMIT_LOAD_EXTENSION \
    -DSQLITE_THREADSAFE=1 -DSQLITE_MAX_MMAP_SIZE=0 -DSQLITE_OMIT_WAL -DHAVE_USLEEP=1 \
    -c "$dir/sqlite3.c" -o "$dir/sqlite3.o"
  "$AR" rcs "$P/lib/libsqlite3.a" "$dir/sqlite3.o"
  cp "$dir/sqlite3.h" "$dir/sqlite3ext.h" "$P/include/"
}

build_cairo() {
  fetch https://cairographics.org/releases/cairo-1.16.0.tar.xz cairo-1.16.0
  # The image surface with FreeType fonts (selected through fontconfig, as
  # WebCore's FreeType code does); WebCore paints into memory and the port
  # copies the result to the screen.
  (cd "$src/cairo-1.16.0" && CFLAGS="$cflags" ./configure --host=arm-linux-gnueabi \
     --prefix="$P" --enable-static --disable-shared --enable-ft --enable-fc --enable-png \
     --disable-xlib --disable-xcb --disable-quartz --disable-win32 \
     --disable-gl --disable-glesv2 --disable-egl --disable-script --disable-ps \
     --disable-pdf --disable-svg --disable-interpreter --disable-trace \
     --disable-gobject --disable-valgrind --disable-full-testing)
  make -C "$src/cairo-1.16.0/src" -j"$JOBS"
  make -C "$src/cairo-1.16.0/src" install
}

build_harfbuzz() {
  fetch https://github.com/harfbuzz/harfbuzz/releases/download/2.7.4/harfbuzz-2.7.4.tar.xz harfbuzz-2.7.4
  # HarfBuzz turns warnings into errors with pragmas that current Clang
  # widens (-Wcast-function-type covers FreeType's finalizer casts).
  cflags="$cflags -DHB_NO_PRAGMA_GCC_DIAGNOSTIC_ERROR" \
  PKG_CONFIG_PATH=$P/lib/pkgconfig:$ICU_DIR/install/lib/pkgconfig cmake_build harfbuzz-2.7.4 \
    -DHB_HAVE_FREETYPE=ON -DHB_HAVE_ICU=ON -DHB_HAVE_GLIB=OFF -DHB_HAVE_GOBJECT=OFF \
    -DHB_BUILD_UTILS=OFF -DHB_BUILD_SUBSET=OFF -DHB_BUILD_TESTS=OFF \
    -DFREETYPE_INCLUDE_DIRS="$sdk_portable/freetype" \
    -DFREETYPE_LIBRARY="$sdk_lib/libsymbian_portable_freetype.a" \
    -DICU_INCLUDE_DIR="$ICU_DIR/install/include"
}

build_libxml2() {
  fetch https://download.gnome.org/sources/libxml2/2.9/libxml2-2.9.12.tar.xz libxml2-2.9.12
  cmake_build libxml2-2.9.12 -DLIBXML2_WITH_ICONV=OFF -DLIBXML2_WITH_ICU=OFF \
    -DLIBXML2_WITH_LZMA=OFF -DLIBXML2_WITH_PYTHON=OFF -DLIBXML2_WITH_PROGRAMS=OFF \
    -DLIBXML2_WITH_TESTS=OFF -DLIBXML2_WITH_HTTP=OFF -DLIBXML2_WITH_FTP=OFF \
    -DLIBXML2_WITH_MODULES=OFF -DLIBXML2_WITH_LEGACY=OFF \
    -DZLIB_INCLUDE_DIR="$sdk_portable/zlib" -DZLIB_LIBRARY="$sdk_lib/libsymbian_portable_zlib.a"
}

build_libxslt() {
  fetch https://download.gnome.org/sources/libxslt/1.1/libxslt-1.1.35.tar.xz libxslt-1.1.35
  # XSLT for WebCore (XSLTProcessorLibxslt), which links libxslt only.
  # The libraries only: xsltproc has no option and needs libm and libdl.
  sed -i '/^add_executable(xsltproc/,/^install(TARGETS xsltproc/d' "$src/libxslt-1.1.35/CMakeLists.txt"
  cmake_build libxslt-1.1.35 -DLIBXSLT_WITH_CRYPTO=OFF -DLIBXSLT_WITH_DEBUGGER=OFF \
    -DLIBXSLT_WITH_MODULES=OFF -DLIBXSLT_WITH_PROFILER=OFF -DLIBXSLT_WITH_XSLT_DEBUG=OFF \
    -DLIBXSLT_WITH_PYTHON=OFF -DLIBXSLT_WITH_TESTS=OFF \
    -DZLIB_INCLUDE_DIR="$sdk_portable/zlib" -DZLIB_LIBRARY="$sdk_lib/libsymbian_portable_zlib.a"
}

build_fontconfig() {
  fetch https://www.freedesktop.org/software/fontconfig/release/fontconfig-2.13.94.tar.xz fontconfig-2.13.94
  # WebCore's FreeType font code selects fonts through fontconfig. The phone's
  # fonts are in Z:\resource\fonts; the configuration and cache live on C:.
  # Configuration files are parsed with libxml2 (built above) instead of expat.
  # Open C's sys/time.h has no BSD timercmp.
  cat > "$src/fontconfig-2.13.94/symbian-compat.h" <<'EOF2'
#include <sys/time.h>
#ifndef timercmp
#define timercmp(a, b, CMP) (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec CMP (b)->tv_usec) : ((a)->tv_sec CMP (b)->tv_sec))
#endif
EOF2
  # Paths start with a drive letter, as on Windows; fontconfig would take
  # "Z:/resource/fonts" for a relative path and prefix the working directory.
  python3 - "$src/fontconfig-2.13.94/src/fcstr.c" <<'EOF2'
import sys
path = sys.argv[1]
s = open(path).read()
s = s.replace("""#ifdef _WIN32
    if (*s == '\\\\' ||""", """#if defined(_WIN32) || defined(__SYMBIAN32__)
    if (*s == '\\\\' ||""", 1)
s = s.replace("""#else
    if (s[0] == '/')
	return FcStrCanonAbsoluteFilename (s);""", """#else
    if (s[0] == '/' || (isalpha (s[0]) && s[1] == ':' && s[2] == '/'))
	return FcStrCanonAbsoluteFilename (s);""", 1)
open(path, "w").write(s)
EOF2
  (cd "$src/fontconfig-2.13.94" && CFLAGS="$cflags -include $src/fontconfig-2.13.94/symbian-compat.h" LIBXML2_CFLAGS="-I$P/include/libxml2" \
     LIBXML2_LIBS="-L$P/lib -lxml2" ./configure --host=arm-linux-gnueabi --prefix="$P" \
     --enable-static --disable-shared --enable-libxml2 --disable-docs --disable-cache-build \
     --disable-nls --with-default-fonts=Z:/resource/fonts \
     --with-baseconfigdir=C:/resource/fontconfig --with-configdir=C:/resource/fontconfig/conf.d \
     --with-templatedir=C:/resource/fontconfig/conf.avail \
     --with-cache-dir=C:/Data/fontconfig-cache)
  make -C "$src/fontconfig-2.13.94/src" -j"$JOBS"
  make -C "$src/fontconfig-2.13.94/src" install
  make -C "$src/fontconfig-2.13.94" install-pkgconfigDATA
  make -C "$src/fontconfig-2.13.94/fontconfig" install
}

build_libpsl() {
  fetch https://github.com/rockdaboot/libpsl/releases/download/0.21.1/libpsl-0.21.1.tar.gz libpsl-0.21.1
  # The public suffix list is compiled in (no data files on the phone); ICU
  # handles internationalized domain names.
  (cd "$src/libpsl-0.21.1" && CFLAGS="$cflags" \
     PKG_CONFIG_PATH="$P/lib/pkgconfig:$ICU_DIR/install/lib/pkgconfig" ./configure \
     --host=arm-linux-gnueabi --prefix="$P" --enable-static --disable-shared \
     --enable-builtin=libicu --enable-runtime=libicu --disable-man --disable-gtk-doc)
  make -C "$src/libpsl-0.21.1/src" -j"$JOBS"
  make -C "$src/libpsl-0.21.1/src" install
  make -C "$src/libpsl-0.21.1/include" install
  make -C "$src/libpsl-0.21.1" install-pkgconfigDATA
}

build_webp() {
  fetch https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.2.0.tar.gz libwebp-1.2.0
  # No SIMD: ARM11 has no NEON.
  cmake_build libwebp-1.2.0 -DWEBP_ENABLE_SIMD=OFF -DWEBP_BUILD_ANIM_UTILS=OFF \
    -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF \
    -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF \
    -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF
}

build_mbedtls_fs() {
  # The SDK's mbedTLS lacks its file-loading functions; curl needs them.
  local tls=$sdk_lib/mbedtls
  mkdir -p "$DEPS_DIR/mbedtls-fs"
  "$CC" $cflags $tls_flags -I"$tls/include" -c "$REPO/shims/mbedtls-fs.c" -o "$DEPS_DIR/mbedtls-fs/mbedtls-fs.o"
  "$AR" rcs "$P/lib/libmbedtls_fs.a" "$DEPS_DIR/mbedtls-fs/mbedtls-fs.o"
}

build_curl() {
  fetch https://curl.se/download/curl-7.88.1.tar.xz curl-7.88.1
  local tls=$sdk_lib/mbedtls
  # The CA bundle is read by shims/mbedtls-fs.c, the SDK's mbedTLS having no
  # file functions (no MBEDTLS_FS_IO).
  local vtls=$src/curl-7.88.1/lib/vtls/mbedtls.c
  grep -q 'mbedtls-fs.h' "$vtls" || sed -i -e 's/^#ifdef MBEDTLS_FS_IO$/#if defined(MBEDTLS_FS_IO) || defined(__SYMBIAN32__)/' \
    -e 's|^#include <mbedtls/sha256.h>$|&\n#ifdef __SYMBIAN32__\n#include "mbedtls-fs.h"\n#endif|' "$vtls"
  grep -q 'mbedtls-fs.h' "$vtls"
  # The SDK's mbedTLS verifies signatures through PSA (MBEDTLS_USE_PSA_CRYPTO),
  # which curl 7.88 never initializes: every handshake failed with
  # MBEDTLS_ERR_PK_BAD_INPUT_DATA (PSA_ERROR_BAD_STATE).
  grep -q 'psa_crypto_init' "$vtls" || perl -0pi -e 's{(static int mbedtls_init\(void\)\n\{\n)}{$1#ifdef MBEDTLS_USE_PSA_CRYPTO\n  if(psa_crypto_init() != PSA_SUCCESS)\n    return 0;\n#endif\n}; s{(#include <mbedtls/sha256.h>\n)}{$1#ifdef MBEDTLS_USE_PSA_CRYPTO\n#include <psa/crypto.h>\n#endif\n}' "$vtls"
  grep -q 'psa_crypto_init' "$vtls"
  # Every connection uses one parsed copy of the CA bundle (shims/mbedtls-fs.c)
  # instead of parsing its own.
  grep -q 'mbedtls_fs_shared_ca_chain' "$vtls" || perl -0pi -e 's{(  mbedtls_x509_crt_init\(&backend->cacert\);\n)}{$1  mbedtls_x509_crt *cachain = &backend->cacert;\n}; s{mbedtls_x509_crt_parse_file\(&backend->cacert, ssl_cafile\)}{mbedtls_fs_shared_ca_chain(ssl_cafile, &cachain)}; s{(mbedtls_ssl_conf_ca_chain\(&backend->config,\n\s*)&backend->cacert,}{$1cachain,}' "$vtls"
  [ "$(grep -c 'cachain' "$vtls")" = 3 ]
  # file: URLs name drive letters (file:///C:/Data/page.html), as on Windows.
  sed -i 's/^#if defined(WIN32) || defined(MSDOS) || defined(__EMX__)$/& || defined(__SYMBIAN32__)/' \
    "$src/curl-7.88.1/lib/file.c"
  sed -i 's/^#if !defined(MSDOS) \&\& !defined(WIN32) \&\& !defined(__CYGWIN__)$/& \&\& !defined(__SYMBIAN32__)/' \
    "$src/curl-7.88.1/lib/urlapi.c"
  # TLS from the SDK's mbedTLS 3.4. HTTP(S) and local files (WebKit's curl
  # backend loads file: URLs through curl too). Open C has no socketpair(),
  # and curl's stand-in (a TCP connection to itself over the loopback) fails
  # there, which failed every host name lookup before the resolver thread
  # started; without it the resolver thread is polled. No default CA bundle
  # or path, which curl would otherwise take from the build machine
  # (/etc/ssl/certs, missing on phones, failed every HTTPS load); WebCore
  # sets the bundle.
  cflags="$cflags $tls_flags" cmake_build curl-7.88.1 -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF \
    -DCURL_DISABLE_DICT=ON -DCURL_DISABLE_FTP=ON -DCURL_DISABLE_GOPHER=ON \
    -DCURL_DISABLE_IMAP=ON -DCURL_DISABLE_LDAPS=ON -DCURL_DISABLE_MQTT=ON \
    -DCURL_DISABLE_POP3=ON -DCURL_DISABLE_RTSP=ON -DCURL_DISABLE_SMB=ON \
    -DCURL_DISABLE_SMTP=ON -DCURL_DISABLE_TELNET=ON -DCURL_DISABLE_TFTP=ON \
    -DCURL_USE_OPENSSL=OFF -DCURL_USE_MBEDTLS=ON -DCURL_USE_LIBSSH2=OFF \
    -DUSE_LIBIDN2=OFF -DCURL_DISABLE_LDAP=ON -DENABLE_IPV6=OFF \
    -DENABLE_THREADED_RESOLVER=ON -DCURL_DISABLE_SOCKETPAIR=ON -DCURL_ZLIB=ON \
    -DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF \
    -DMBEDTLS_INCLUDE_DIRS="$tls/include" -DMBEDTLS_LIBRARY="$tls/lib/libmbedtls.a" \
    -DMBEDX509_LIBRARY="$tls/lib/libmbedx509.a" -DMBEDCRYPTO_LIBRARY="$tls/lib/libmbedcrypto.a" \
    -DZLIB_INCLUDE_DIR="$sdk_portable/zlib" -DZLIB_LIBRARY="$sdk_lib/libsymbian_portable_zlib.a"
}

# One library per process, so set -e applies inside build_NAME (it does not in
# a command list joined with ||).
if [ "${1:-}" = --one ]; then
  set -e
  "build_$2"
  exit
fi
all=(pixman sqlite libxml2 libxslt fontconfig cairo harfbuzz libpsl webp mbedtls_fs curl)
sdk_pkgconfig
for name in "${@:-${all[@]}}"; do
  step "$name"
  "$0" --one "$name" > "$DEPS_DIR/$name.log" 2>&1 || { tail -30 "$DEPS_DIR/$name.log"; exit 1; }
done
ls -la "$P/lib"
