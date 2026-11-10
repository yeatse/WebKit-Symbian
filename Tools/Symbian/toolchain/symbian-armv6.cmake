# CMake toolchain for building WebKit sources with the symbian-platform SDK.
# CMake treats the target as a Unix-like system; WTF identifies Symbian from
# the compiler's __SYMBIAN32__ macro. Executables linked by CMake here are
# throwaway ELFs for configure checks; images are linked with the SDK flow.
set(CMAKE_SYSTEM_NAME Linux)
# Selects the OS(SYMBIAN) sources in the WebKit CMake files.
set(SYMBIAN TRUE)
set(CMAKE_SYSTEM_PROCESSOR armv6)
set(_dir "${CMAKE_CURRENT_LIST_DIR}")
set(CMAKE_C_COMPILER "${_dir}/sym-cc")
set(CMAKE_CXX_COMPILER "${_dir}/sym-c++")
# The compiler wrappers read the SDK location from the environment too.
set(SYMBIAN_SDK_PREFIX "$ENV{SYMBIAN_SDK_PREFIX}")
if(NOT SYMBIAN_SDK_PREFIX)
  message(FATAL_ERROR "Set SYMBIAN_SDK_PREFIX to the symbian-platform SDK")
endif()
set(CMAKE_AR "${SYMBIAN_SDK_PREFIX}/bin/llvm-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${SYMBIAN_SDK_PREFIX}/bin/llvm-ranlib" CACHE FILEPATH "")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Every Symbian^3 ARM11 has VFPv2. softfp preserves the SDK and OS calling
# convention while permitting hardware floating-point instructions.
set(symbian_fp "-mfloat-abi=softfp -mfpu=vfpv2")
set(CMAKE_C_FLAGS_INIT "${symbian_fp}")
set(CMAKE_ASM_FLAGS_INIT "${symbian_fp}")
set(CMAKE_CXX_FLAGS_INIT "${symbian_fp}")
set(CMAKE_CXX_STANDARD 23)
