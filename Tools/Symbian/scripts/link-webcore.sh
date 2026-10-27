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
out=$WEBCORE_BUILD/linktest
mkdir -p "$out"
rm -f "$out/linktest.elf"
echo 'int main() { return 0; }' > "$out/main.cpp"
"$REPO/toolchain/sym-c++" -mfloat-abi=softfp -mfpu=vfpv2 "$out/main.cpp" -o "$out/linktest.elf" \
  -Wl,--whole-archive "$L/libWebCore.a" -Wl,--no-whole-archive \
  "$L/libPAL.a" "$L/libJavaScriptCore.a" "$L/libWTF.a" \
  "$D/libcairo.a" "$D/libpixman-1.a" "$D/libharfbuzz-icu.a" "$D/libharfbuzz.a" \
  "$D/libfontconfig.a" "$D/libxml2.a" "$D/libsqlite3.a" "$D/libpsl.a" \
  "$D/libwebpdemux.a" "$D/libwebp.a" "$D/libcurl.a" "$D/libmbedtls_fs.a" \
  "$M/libmbedtls.a" "$M/libmbedx509.a" "$M/libmbedcrypto.a" \
  "$I/libicui18n.a" "$I/libicuuc.a" "$I/libicudata.a" \
  "$S/libsymbian_portable_freetype.a" "$S/libsymbian_portable_png.a" \
  "$S/libsymbian_portable_jpeg.a" "$S/libsymbian_portable_zlib.a" \
  -Wl,--error-limit=0 2>&1 | { grep -oE "undefined symbol: .*" || true; } | sort -u
[ -f "$out/linktest.elf" ] && command -v llvm-size >/dev/null && llvm-size "$out/linktest.elf"
