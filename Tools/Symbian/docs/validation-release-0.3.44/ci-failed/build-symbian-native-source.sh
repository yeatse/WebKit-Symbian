#!/usr/bin/env bash
# Builds symbian-native from symbian-platform 0.3.0 with
# patches/symbian-native (large images/unwind tables and validated LLD thunks).
. "$(dirname "$0")/common.sh"
mkdir -p "$WORK"
JSON=${NLOHMANN_JSON_SRC:-$WORK/src/nlohmann-json}
SYMBIAN_NATIVE_TESTS=${SYMBIAN_NATIVE_TESTS:-ON}
if [ ! -d "$SYMBIAN_PLATFORM_SRC/.git" ]; then
  git clone -q --depth 1 --branch v0.3.0 \
    https://github.com/hpnkv/symbian-platform "$SYMBIAN_PLATFORM_SRC"
fi
if [ ! -d "$JSON" ]; then
  git clone -q --depth 1 --branch v3.12.0 https://github.com/nlohmann/json "$JSON"
fi
# The repository's host dependencies (static OpenSSL, libusb, Boost).
export SYMBIAN_DEPS_PREFIX=${SYMBIAN_DEPS_PREFIX:-$SYMBIAN_PLATFORM_SRC/.symbian/host-deps}
(cd "$SYMBIAN_PLATFORM_SRC" &&
  CMAKE_BUILD_PARALLEL_LEVEL="$JOBS" scripts/bootstrap_wheel_deps.sh > "$WORK/symbian-native-deps.log" 2>&1)
for patch in "$REPO"/patches/symbian-native/*.patch; do
  if ! git -C "$SYMBIAN_PLATFORM_SRC" apply --reverse --check "$patch" 2>/dev/null; then
    git -C "$SYMBIAN_PLATFORM_SRC" apply "$patch"
  fi
done
test_options=()
if [ -f "$SDK/host/lib/cmake/GTest/GTestConfig.cmake" ]; then
  test_options+=(-DGTest_DIR="$SDK/host/lib/cmake/GTest")
fi
cmake -S "$SYMBIAN_PLATFORM_SRC" -B "$SYMBIAN_NATIVE_BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSYMBIAN_BUILD_PYTHON=OFF \
  -DSYMBIAN_BUILD_GUI_EXAMPLE=OFF -DSYMBIAN_BUILD_NATIVE_EXAMPLES=OFF \
  -DBUILD_TESTING="$SYMBIAN_NATIVE_TESTS" "${test_options[@]}" \
  -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON="$JSON" \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ > "$WORK/symbian-native-configure.log" 2>&1
ninja -C "$SYMBIAN_NATIVE_BUILD" -j "$JOBS" symbian_native_tool > "$WORK/symbian-native-build.log" 2>&1
if [ "$SYMBIAN_NATIVE_TESTS" = ON ]; then
  ninja -C "$SYMBIAN_NATIVE_BUILD" -j "$JOBS" symbian_e32_test >> "$WORK/symbian-native-build.log" 2>&1
  ctest --test-dir "$SYMBIAN_NATIVE_BUILD" --output-on-failure \
    -R '^symbian_e32_test$' | tee "$WORK/symbian-native-tests.log"
fi
ls -la "$SYMBIAN_NATIVE"
