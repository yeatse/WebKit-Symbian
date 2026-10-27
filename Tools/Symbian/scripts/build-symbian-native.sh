#!/usr/bin/env bash
# Builds symbian-native from symbian-platform 0.3.0 with
# patches/symbian-native (raised import, GOT-free relocation and absolute
# pointer limits; images within the old limits convert unchanged).
. "$(dirname "$0")/common.sh"
JSON=${NLOHMANN_JSON_SRC:-$WORK/src/nlohmann-json}
if [ ! -d "$SYMBIAN_PLATFORM_SRC/.git" ]; then
  git clone -q --depth 1 --branch v0.3.0 \
    https://github.com/hpnkv/symbian-platform "$SYMBIAN_PLATFORM_SRC"
fi
if [ ! -d "$JSON" ]; then
  git clone -q --depth 1 --branch v3.12.0 https://github.com/nlohmann/json "$JSON"
fi
# The repository's host dependencies (static OpenSSL, libusb, Boost).
export SYMBIAN_DEPS_PREFIX=${SYMBIAN_DEPS_PREFIX:-$SYMBIAN_PLATFORM_SRC/.symbian/host-deps}
if [ ! -f "$SYMBIAN_DEPS_PREFIX/lib/libcrypto.a" ]; then
  (cd "$SYMBIAN_PLATFORM_SRC" && scripts/bootstrap_wheel_deps.sh > /dev/null)
fi
for patch in "$REPO"/patches/symbian-native/*.patch; do
  if ! git -C "$SYMBIAN_PLATFORM_SRC" apply --reverse --check "$patch" 2>/dev/null; then
    git -C "$SYMBIAN_PLATFORM_SRC" apply "$patch"
  fi
done
cmake -S "$SYMBIAN_PLATFORM_SRC" -B "$SYMBIAN_NATIVE_BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSYMBIAN_BUILD_PYTHON=OFF \
  -DSYMBIAN_BUILD_GUI_EXAMPLE=OFF -DSYMBIAN_BUILD_NATIVE_EXAMPLES=OFF \
  -DBUILD_TESTING=OFF -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON="$JSON" \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ > /dev/null
ninja -C "$SYMBIAN_NATIVE_BUILD" symbian_native_tool > /dev/null
ls -la "$SYMBIAN_NATIVE"
