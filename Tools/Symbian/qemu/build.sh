#!/usr/bin/env bash
# Builds this checkout's JavaScriptCore for Linux ARMv6 and checks it under
# qemu-arm -cpu arm1136, the ARM11 core of Symbian^3 phones. Only the CPU
# backend is exercised here; the Symbian platform layer needs a device. Run
# ../scripts/build-icu.sh (for the host ICU tools and data) first.
# Needs qemu-user and g++-arm-linux-gnueabi (Ubuntu/Debian packages).
. "$(dirname "$0")/../scripts/common.sh"
Q=$WORK/qemu
ICU=$Q/icu
host=$ICU_DIR/host
arm11="-march=armv6 -marm -mfpu=vfp -mfloat-abi=softfp"
mkdir -p "$Q/icu-build" "$Q/icudata"
if [ ! -f "$ICU/lib/libicuuc.a" ]; then
  (cd "$Q/icu-build" && CC=arm-linux-gnueabi-gcc CXX=arm-linux-gnueabi-g++ \
     CFLAGS="-O2 $arm11" CXXFLAGS="-O2 -std=c++11 $arm11" \
     "$ICU_DIR/src/icu4c/source/configure" --host=arm-linux-gnueabi \
       --with-cross-build="$host" --disable-shared --enable-static \
       --disable-tests --disable-samples --disable-extras --disable-tools \
       --disable-layoutex --with-data-packaging=static --prefix="$ICU" \
     && make -j"$JOBS" && make install) > "$Q/icu.log" 2>&1
  # make install leaves ICU's empty stub data object behind; replace the
  # archive, or the linker picks the stub and every Intl API fails.
  LD_LIBRARY_PATH="$host/lib" "$host/bin/genccode" -a gcc -e icudt70 \
    -d "$Q/icudata" "$host/data/out/tmp/icudt70l.dat" > /dev/null
  arm-linux-gnueabi-gcc -c "$Q/icudata/icudt70l_dat.S" -o "$Q/icudata/icudt70_dat.o"
  rm -f "$ICU/lib/libicudata.a"
  arm-linux-gnueabi-ar rcs "$ICU/lib/libicudata.a" "$Q/icudata/icudt70_dat.o"
fi
cmake -S "$WEBKIT_SRC" -B "$Q/build" -G Ninja -DPORT=JSCOnly \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$REPO/qemu/toolchain-linux-armv6.cmake" \
  "-DCMAKE_FIND_ROOT_PATH=/usr/arm-linux-gnueabi;$ICU" \
  -DENABLE_STATIC_JSC=ON -DUSE_SYSTEM_MALLOC=ON -DENABLE_API_TESTS=OFF \
  -DDEVELOPER_MODE=OFF -DENABLE_JIT=OFF -DENABLE_DFG_JIT=OFF -DENABLE_C_LOOP=ON \
  -DENABLE_SAMPLING_PROFILER=OFF -DENABLE_WEBASSEMBLY=OFF -DENABLE_FTL_JIT=OFF \
  -DICU_INCLUDE_DIR="$ICU/include" -DICU_UC_LIBRARY_RELEASE="$ICU/lib/libicuuc.a" \
  -DICU_I18N_LIBRARY_RELEASE="$ICU/lib/libicui18n.a" \
  -DICU_DATA_LIBRARY_RELEASE="$ICU/lib/libicudata.a" > "$Q/configure.log"
ninja -C "$Q/build" -j"$JOBS" jsc > "$Q/build.log"
run=(qemu-arm -cpu arm1136 -L /usr/arm-linux-gnueabi "$Q/build/bin/jsc")
"${run[@]}" "$REPO/tests/es2020.js"
"${run[@]}" "$REPO/tests/intl.js"
"${run[@]}" --useSharedArrayBuffer=true "$REPO/tests/resizable-arraybuffer.js"
