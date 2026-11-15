#!/usr/bin/env bash
# Builds tests/gl-device-probe and packages it as glprobe.sis, after build-webcore.sh.
. "$(dirname "$0")/common.sh"
require_sdk
build=$WORK/build/gl-probe
"$SDK/bin/cmake" -S "$REPO/tests/gl-device-probe" -B "$build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$SDK/cmake/symbian-arm.cmake" -DSYMBIAN_SDK_PREFIX="$SDK" \
  -DCMAKE_MAKE_PROGRAM="$SDK/bin/ninja" -DCMAKE_BUILD_TYPE=Release \
  -DSYMBIAN_NATIVE_CONVERTER="$SYMBIAN_NATIVE" \
  -DWEBKIT_SOURCE_DIR="$WEBKIT_SRC" -DWEBKIT_BUILD_DIR="$WEBCORE_BUILD" \
  -DDEPS_DIR="$DEPS_DIR/install" \
  -DJPEG_LIBRARY="${SYMBIAN_JPEG_PREFIX:-$DEPS_DIR/install}/lib/libjpeg.a" 2>&1 | tee "$WORK/gl-probe-configure.log"
"$SDK/bin/ninja" -j"$JOBS" -C "$build" 2>&1 | tee "$WORK/gl-probe-build.log"
python3 "$REPO/scripts/e32-header.py" "$build/e32/glprobe.exe" 0x100000 0x2000000 0x80000 --fpu=vfpv2

sign=()
if [ -n "${SIS_CERTIFICATE:-}" ] || [ -n "${SIS_KEY:-}" ]; then
  sign=(--certificate "${SIS_CERTIFICATE:?SIS_KEY needs SIS_CERTIFICATE}" --key "${SIS_KEY:?SIS_CERTIFICATE needs SIS_KEY}")
fi
mkdir -p "$WORK/sis"
python3 "$REPO/scripts/make-sis.py" \
  --uid 0xE1A5C0F1 --name "glprobe" --vendor symbian-webkit \
  --version "${SIS_VERSION:-0.3.0}" --output "$WORK/sis/glprobe.sis" \
  --platform 0x20022E6D=S60ProductID --platform 0x20032DE7=S60ProductID "${sign[@]}" \
  --file "$build/e32/glprobe.exe=c:\\sys\\bin\\glprobe.exe" \
  --file "$build/e32/glprobe_reg.rsc=c:\\private\\10003a3f\\import\\apps\\glprobe_reg.rsc"
ls -l "$WORK/sis/glprobe.sis"
