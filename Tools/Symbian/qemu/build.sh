#!/usr/bin/env bash
# Builds this checkout's JavaScriptCore for Linux ARMv6 and checks it under
# qemu-arm -cpu arm1136, the ARM11 core of Symbian^3 phones. Only the CPU
# backend is exercised here; the Symbian platform layer needs a device. Run
# ../scripts/build-icu.sh (for the host ICU tools and data) first.
# prepare-tools.sh supplies isolated Debian tools. --dependencies-only builds
# Linux ICU without configuring or starting the JavaScriptCore build.
. "$(dirname "$0")/../scripts/common.sh"
Q=$WORK/qemu
[ ! -f "$Q/env.sh" ] || . "$Q/env.sh"
ICU=$Q/icu
host=${ICU_HOST_DIR:-$ICU_DIR/host}
icu_source=${ICU_SOURCE_DIR:-$ICU_DIR/src/icu4c/source}
linux_sysroot=${ARM11_LINUX_SYSROOT:-/usr/arm-linux-gnueabi}
compiler_sysroot=${ARM11_TOOLS_ROOT:-/}
arm11="-march=armv6 -marm -mfpu=vfp -mfloat-abi=softfp"
mkdir -p "$Q/icu-build" "$Q/icudata"
if [ ! -f "$ICU/lib/libicuuc.a" ]; then
  (cd "$Q/icu-build" && CC="arm-linux-gnueabi-gcc --sysroot=$compiler_sysroot" CXX="arm-linux-gnueabi-g++ --sysroot=$compiler_sysroot" \
     CFLAGS="-O2 $arm11" CXXFLAGS="-O2 -std=c++11 $arm11" \
     "$icu_source/configure" --host=arm-linux-gnueabi \
       --with-cross-build="$host" --disable-shared --enable-static \
       --disable-tests --disable-samples --disable-extras --disable-tools \
       --disable-layoutex --with-data-packaging=static --prefix="$ICU" \
     && make -j"$JOBS" && make install) > "$Q/icu.log" 2>&1
  # make install leaves ICU's empty stub data object behind; replace the
  # archive, or the linker picks the stub and every Intl API fails.
  LD_LIBRARY_PATH="$host/lib" "$host/bin/genccode" -a gcc -e icudt70 \
    -d "$Q/icudata" "$host/data/out/tmp/icudt70l.dat" > /dev/null
  arm-linux-gnueabi-gcc --sysroot="$compiler_sysroot" $arm11 -c "$Q/icudata/icudt70l_dat.S" -o "$Q/icudata/icudt70_dat.o"
  rm -f "$ICU/lib/libicudata.a"
  arm-linux-gnueabi-ar rcs "$ICU/lib/libicudata.a" "$Q/icudata/icudt70_dat.o"
fi
if [ "${1:-}" = --dependencies-only ]; then
  printf 'Linux ARMv6 ICU ready: %s\n' "$ICU"
  exit 0
fi
cmake -S "$WEBKIT_SRC" -B "$Q/build" -G Ninja -DPORT=JSCOnly \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$REPO/qemu/toolchain-linux-armv6.cmake" \
  "-DCMAKE_FIND_ROOT_PATH=$linux_sysroot;$ICU" \
  -DENABLE_STATIC_JSC=ON -DUSE_SYSTEM_MALLOC=ON -DENABLE_API_TESTS=OFF \
  -DDEVELOPER_MODE=OFF "${jit_flags[@]}" \
  -DENABLE_SAMPLING_PROFILER=OFF -DENABLE_WEBASSEMBLY=OFF -DENABLE_FTL_JIT=OFF \
  -DICU_INCLUDE_DIR="$ICU/include" -DICU_UC_LIBRARY_RELEASE="$ICU/lib/libicuuc.a" \
  -DICU_I18N_LIBRARY_RELEASE="$ICU/lib/libicui18n.a" \
  -DICU_DATA_LIBRARY_RELEASE="$ICU/lib/libicudata.a" > "$Q/configure.log"
if [ "${CONFIGURE_ONLY:-0}" = 1 ]; then
  printf 'Linux ARMv6 JSC configured: %s/build\n' "$Q"
  exit 0
fi
ninja -C "$Q/build" -j"$JOBS" jsc > "$Q/build.log"
python3 "$REPO/qemu/run-jit-tests.py" "$Q/build/bin/jsc" \
  --qemu "${QEMU_ARM:-qemu-arm}" --sysroot "$linux_sysroot"
