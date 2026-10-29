#!/usr/bin/env bash
# Configures and builds the Symbian port (static WTF, JavaScriptCore, PAL and
# WebCore) for ARMv6 against the SDK's FreeType, libpng, libjpeg, zlib and
# mbedTLS and the libraries from build-deps.sh.
. "$(dirname "$0")/common.sh"
require_sdk
I=$ICU_DIR/install
D=$DEPS_DIR/install
sdk_portable=$SDK/include/portable
sdk_lib=$SDK/lib/armv6
build=$WEBCORE_BUILD
export PKG_CONFIG_PATH=$D/lib/pkgconfig:$I/lib/pkgconfig PKG_CONFIG_LIBDIR=$D/lib/pkgconfig:$I/lib/pkgconfig
cmake -S "$WEBKIT_SRC" -B "$build" -G Ninja -DPORT=Symbian \
  -DCMAKE_BUILD_TYPE=Release "${release_flags[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="$REPO/toolchain/symbian-armv6.cmake" \
  -DCMAKE_FIND_ROOT_PATH="$D;$I" -DCMAKE_PREFIX_PATH="$D;$I" \
  -DDEVELOPER_MODE=OFF \
  -DICU_INCLUDE_DIR="$I/include" -DICU_UC_LIBRARY_RELEASE="$I/lib/libicuuc.a" \
  -DICU_I18N_LIBRARY_RELEASE="$I/lib/libicui18n.a" -DICU_DATA_LIBRARY_RELEASE="$I/lib/libicudata.a" \
  -DFREETYPE_INCLUDE_DIRS="$sdk_portable/freetype" -DFREETYPE_LIBRARY="$sdk_lib/libsymbian_portable_freetype.a" \
  -DPNG_PNG_INCLUDE_DIR="$sdk_portable/png" -DPNG_LIBRARY="$sdk_lib/libsymbian_portable_png.a" \
  -DJPEG_INCLUDE_DIR="$sdk_portable/jpeg" -DJPEG_LIBRARY="$sdk_lib/libsymbian_portable_jpeg.a" \
  -DZLIB_INCLUDE_DIR="$sdk_portable/zlib" -DZLIB_LIBRARY="$sdk_lib/libsymbian_portable_zlib.a" \
  -DLIBXML2_INCLUDE_DIR="$D/include/libxml2" -DLIBXML2_LIBRARY="$D/lib/libxml2.a" \
  -DLIBXSLT_INCLUDE_DIR="$D/include" -DLIBXSLT_LIBRARIES="$D/lib/libxslt.a" \
  -DSQLite3_INCLUDE_DIR="$D/include" -DSQLite3_LIBRARY="$D/lib/libsqlite3.a" \
  "$@" > "$WORK/webcore-configure.log"
ninja -C "$build" -k 0 -j"$JOBS" > "$WORK/webcore-build.log"
ls -la "$build/lib"
