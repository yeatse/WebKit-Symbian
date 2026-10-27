#!/usr/bin/env bash
# Configures and builds the JSCOnly port (static JavaScriptCore + jsc shell)
# for ARMv6 with LLInt, baseline JIT, DFG JIT and YARR JIT.
. "$(dirname "$0")/common.sh"
require_sdk
I=$ICU_DIR/install
cmake -S "$WEBKIT_SRC" -B "$WEBKIT_BUILD" -G Ninja -DPORT=JSCOnly \
  -DCMAKE_BUILD_TYPE=Release "${release_flags[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="$REPO/toolchain/symbian-armv6.cmake" \
  -DENABLE_STATIC_JSC=ON -DUSE_SYSTEM_MALLOC=ON -DENABLE_API_TESTS=OFF \
  -DDEVELOPER_MODE=OFF -DENABLE_JIT=ON -DENABLE_SAMPLING_PROFILER=OFF \
  -DICU_INCLUDE_DIR="$I/include" -DCMAKE_PREFIX_PATH="$I" \
  -DICU_UC_LIBRARY_RELEASE="$I/lib/libicuuc.a" \
  -DICU_I18N_LIBRARY_RELEASE="$I/lib/libicui18n.a" \
  -DICU_DATA_LIBRARY_RELEASE="$I/lib/libicudata.a" \
  -DENABLE_C_LOOP=OFF -DENABLE_DFG_JIT=ON -DENABLE_FTL_JIT=OFF \
  -DENABLE_WEBASSEMBLY=OFF "$@" > "$WORK/jsc-configure.log"
ninja -C "$WEBKIT_BUILD" -j"$JOBS" > "$WORK/jsc-build.log"
grep -E "ENABLE_(JIT|DFG_JIT|FTL_JIT|C_LOOP|WEBASSEMBLY|INTL) " "$WEBKIT_BUILD/cmakeconfig.h"
ls -la "$WEBKIT_BUILD/lib"
