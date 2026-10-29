# Linux ARMv6 (ARM state, VFPv2, softfp: the Symbian^3 CPU profile) for
# running the restored ARM_TRADITIONAL backend under qemu-arm -cpu arm1136.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv6)
set(CMAKE_C_COMPILER arm-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabi-g++)
set(arm11 "-march=armv6 -marm -mfpu=vfp -mfloat-abi=softfp")
set(CMAKE_C_FLAGS_INIT "${arm11}")
# WebKit 2.32 predates GCC 13's header cleanups.
set(CMAKE_CXX_FLAGS_INIT "${arm11} -include cstdint -include utility -include iterator")
set(CMAKE_ASM_FLAGS_INIT "${arm11}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
