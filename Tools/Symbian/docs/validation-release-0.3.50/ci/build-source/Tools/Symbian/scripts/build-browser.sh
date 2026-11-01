#!/usr/bin/env bash
# Builds browser.exe (the AVKON shell around the Symbian WebView) with its
# AppArc registration and caption resources, after build-webcore.sh.
#
#   scripts/build-browser.sh            the browser
#   STUB=1 scripts/build-browser.sh     the shell with a stand-in engine
. "$(dirname "$0")/common.sh"
require_sdk
if [ -n "${STUB:-}" ]; then
  build=$WORK/build/browser-stub
  options=(-DBROWSER_STUB_ENGINE=ON)
else
  build=$BROWSER_BUILD
  options=(-DBROWSER_STUB_ENGINE=OFF)
fi
"$SDK/bin/cmake" -S "$REPO/browser" -B "$build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$SDK/cmake/symbian-arm.cmake" -DSYMBIAN_SDK_PREFIX="$SDK" \
  -DCMAKE_MAKE_PROGRAM="$SDK/bin/ninja" -DCMAKE_BUILD_TYPE=Release \
  -DSYMBIAN_NATIVE_CONVERTER="$SYMBIAN_NATIVE" \
  -DWEBKIT_SOURCE_DIR="$WEBKIT_SRC" -DWEBKIT_BUILD_DIR="$WEBCORE_BUILD" \
  -DDEPS_DIR="$DEPS_DIR/install" -DICU_LIBRARY_DIR="$ICU_DIR/install/lib" \
  "${options[@]}" > "$WORK/browser-configure.log"
"$SDK/bin/ninja" -j"$JOBS" -C "$build" > "$WORK/browser-build.log"
# WebKit needs far more than a GUI application's default 1 MB heap, and its
# ARMv6 code requires VFPv2 even when the executable's own ELF attributes omit it.
fpu_flags=()
if [ -z "${STUB:-}" ]; then fpu_flags=(--fpu=vfpv2); fi
python3 "$REPO/scripts/e32-header.py" "$build/e32/browser.exe" \
  "${BROWSER_HEAP_MIN:-0x100000}" "${BROWSER_HEAP_MAX:-0x8000000}" "${BROWSER_STACK:-0x100000}" "${fpu_flags[@]}"
ls -l "$build/e32/"
