# Linux ARMv6 (ARM state, VFPv2, softfp: the Symbian^3 CPU profile) for
# running the ARM-state LLInt, Baseline JIT and DFG under qemu-arm -cpu arm1136.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv6)
set(CMAKE_C_COMPILER arm-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabi-g++)
if(DEFINED ENV{ARM11_TOOLS_ROOT})
    # Debian's cross-linker scripts name /usr/arm-linux-gnueabi/lib explicitly.
    # The compiler sysroot is the unpacked package root; QEMU's -L is its
    # usr/arm-linux-gnueabi subdirectory containing lib/ld-linux.so.3.
    set(CMAKE_SYSROOT "$ENV{ARM11_TOOLS_ROOT}")
endif()
set(arm11 "-march=armv6 -marm -mfpu=vfp -mfloat-abi=softfp")
set(CMAKE_C_FLAGS_INIT "${arm11}")
set(CMAKE_CXX_FLAGS_INIT "${arm11}")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_ASM_FLAGS_INIT "${arm11}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
