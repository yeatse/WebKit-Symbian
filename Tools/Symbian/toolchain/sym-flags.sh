# Target flags of the symbian-platform SDK (streams runtime profile, ARM mode).
# Sourced by sym-cc and sym-c++.
SDK=${SYMBIAN_SDK_PREFIX:?set SYMBIAN_SDK_PREFIX to the symbian-platform SDK}
SYM_DEFS="-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE -D_POSIX_C_SOURCE=200112L -D_UNICODE -D__EABI__ -D__EPOC32__ -D__GCC32__ -D__GCCV3__ -D__LONG_LONG_SUPPORTED -D__MARM_ARMV5__ -D__MARM__ -D__SYMBIAN32__ -DSYMBIAN_RUNTIME_MIMALLOC=1 -D__SOFTFP="
SYM_INC="-isystem $SDK/include/platform -isystem $SDK/include/stream-config -isystem $SDK/include -isystem $SDK/include/config -isystem $SDK/include/c++ -isystem $SDK/include/compiler -isystem $SDK/include/openc -isystem $SDK/include/libm -isystem $SDK/include/libc -isystem $SDK/include/pthread -isystem $SDK/include/posix4"
SYM_ARCH="--target=armv6-none-eabi -marm -march=armv6 -mfpu=none -mfloat-abi=soft -nostdinc -fhosted -fno-builtin -fshort-wchar -fno-pic -ffunction-sections -fdata-sections"
