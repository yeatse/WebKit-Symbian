# Shared settings for the build scripts. Override any of these in the
# environment before running build.sh or an individual step.
set -euo pipefail
# REPO is Tools/Symbian; WEBKIT_SRC is the WebKit checkout around it.
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WEBKIT_SRC=${WEBKIT_SRC:-$(cd "$REPO/../.." && pwd)}
WORK=${SYMBIAN_WEBKIT_WORK:-$WEBKIT_SRC/WebKitBuild/Symbian}
export SYMBIAN_SDK_PREFIX=${SYMBIAN_SDK_PREFIX:-$WORK/symbian-sdk}
SDK=$SYMBIAN_SDK_PREFIX
JOBS=${JOBS:-$(nproc)}

WEBKIT_BUILD=${WEBKIT_BUILD:-$WORK/build/jsc}
E32_BUILD=${E32_BUILD:-$WORK/build/e32}
WEBCORE_BUILD=${WEBCORE_BUILD:-$WORK/build/webcore}
BROWSER_BUILD=${BROWSER_BUILD:-$WORK/build/browser}
ICU_DIR=${ICU_DIR:-$WORK/icu}
DEPS_DIR=${DEPS_DIR:-$WORK/deps}
SYMBIAN_PLATFORM_SRC=${SYMBIAN_PLATFORM_SRC:-$WORK/symbian-platform}
SYMBIAN_NATIVE_BUILD=${SYMBIAN_NATIVE_BUILD:-$WORK/symbian-native}
SYMBIAN_NATIVE=$SYMBIAN_NATIVE_BUILD/cpp/symbian/tools/symbian-native

ICU_TAG=release-70-1
# ICU_DATA=file keeps the ICU data in icudt70l.dat, read at run time from
# ICU_DATA_DIR on the phone; ICU_DATA=static links it into the image.
ICU_DATA=${ICU_DATA:-file}
ICU_DATA_DIR=${ICU_DATA_DIR:-C:/resource/icu}
# Languages whose locale data is kept, or "all".
ICU_LOCALES=${ICU_LOCALES:-"en zh ja ko es fr de ru pt it ar hi tr vi id th nl pl uk ms"}

# Optimization for the code in the images: -Os (default) or -O2/-O3.
# CMake's Release default, -O3, made browser.exe's code 34 MB.
OPTIMIZE=${OPTIMIZE:--Os}
release_flags=(-DCMAKE_C_FLAGS_RELEASE="$OPTIMIZE -DNDEBUG" -DCMAKE_CXX_FLAGS_RELEASE="$OPTIMIZE -DNDEBUG")

step() { printf '\n== %s\n' "$*"; }

require_sdk() {
  if [ ! -x "$SDK/bin/clang++" ] || [ ! -f "$SDK/cmake/symbian-arm.cmake" ]; then
    echo "SYMBIAN_SDK_PREFIX ($SDK) is not a symbian-platform 0.3.0 SDK" >&2
    exit 1
  fi
}
