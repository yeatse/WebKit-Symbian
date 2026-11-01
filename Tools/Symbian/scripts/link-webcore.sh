#!/usr/bin/env bash
# Links all of WebCore (whole archive) with its libraries into one ELF and
# prints the symbols the port still lacks; no output means none.
. "$(dirname "$0")/common.sh"
require_sdk
L=$WEBCORE_BUILD/lib
D=$DEPS_DIR/install/lib
I=$ICU_DIR/install/lib
M=$SDK/lib/armv6/mbedtls/lib
S=$SDK/lib/armv6
# This CLI links archives directly, so it must supply the same configured
# feature dependencies as the independent browser CMake image project.
config=$WEBCORE_BUILD/cmakeconfig.h
if [ ! -f "$config" ]; then
  echo "Configure WebCore before running its whole-archive link check" >&2
  exit 1
fi
feature_libraries=()
if grep -q '^#define USE_TEXTURE_MAPPER 1$' "$config"; then
  feature_libraries+=("$L/libSymbianEGLBridge.a"
    "$SDK/proxies/libegl/libegl.dso" "$SDK/proxies/libglesv2/libglesv2.dso")
fi
if grep -q '^#define ENABLE_WEBGL 1$' "$config"; then
  feature_libraries+=("$L/libGLESv2.a" "$L/libANGLE.a")
fi
if grep -q '^#define ENABLE_WEB_AUDIO 1$' "$config"; then
  feature_libraries+=("$SDK/proxies/mediaclientaudiostream/mediaclientaudiostream.dso")
fi
out=$WEBCORE_BUILD/linktest
mkdir -p "$out"
rm -f "$out/linktest.elf"
echo 'int main() { return 0; }' > "$out/main.cpp"
# Match the real native-leave boundary used by the browser's SDK CMake target.
# WebCore's native observers contain leave-enabled objects even when main is empty.
"$REPO/toolchain/sym-c++" -mfloat-abi=softfp -mfpu=vfpv2 -fno-exceptions -fno-rtti \
  -isystem "$SDK/include/abseil" "$out/main.cpp" \
  "$SDK/cmake/native_leave_personality.cc" "$SDK/cmake/native_leave_personality.S" \
  -Wl,-z,nocopyreloc,-z,notext,--undefined=symbian_native_leave_personalities \
  -o "$out/linktest.elf" \
  -Wl,-Bdynamic,--start-group -Wl,--whole-archive "$L/libWebCore.a" -Wl,--no-whole-archive \
  "$L/libPAL.a" "$L/libJavaScriptCore.a" "$L/libWTF.a" "${feature_libraries[@]}" \
  "$D/libcairo.a" "$D/libpixman-1.a" "$D/libharfbuzz-icu.a" "$D/libharfbuzz.a" \
  "$D/libfontconfig.a" "$D/libxslt.a" "$D/libxml2.a" "$D/libsqlite3.a" "$D/libpsl.a" \
  "$SDK/proxies/esock/esock.dso" "$SDK/proxies/insock/insock.dso" \
  "$SDK/proxies/connmon/connmon.dso" \
  "$D/libwebpdemux.a" "$D/libwebp.a" "$D/libcurl.a" "$D/libnghttp2.a" "$D/libmbedtls_fs.a" \
  "$D/libwoff2dec.a" "$D/libwoff2common.a" "$D/libbrotlidec.a" "$D/libbrotlicommon.a" \
  "$M/libmbedtls.a" "$M/libmbedx509.a" "$M/libmbedcrypto.a" \
  "$I/libicui18n.a" "$I/libicuuc.a" "$I/libicudata.a" \
  "$S/libsymbian_portable_freetype.a" "$S/libsymbian_portable_png.a" \
  "$S/libsymbian_portable_jpeg.a" "$S/libsymbian_portable_zlib.a" \
  -Wl,--end-group -Wl,--error-limit=0 2>&1 | tee "$out/link.log" | { grep -oE "undefined symbol: .*" || true; } | sort -u
test -f "$out/linktest.elf"
if command -v llvm-size >/dev/null; then
  llvm-size "$out/linktest.elf"
fi
