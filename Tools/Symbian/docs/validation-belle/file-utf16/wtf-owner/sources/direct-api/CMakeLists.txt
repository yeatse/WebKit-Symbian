cmake_minimum_required(VERSION 3.28)
project(symbian_file_io_contract LANGUAGES CXX ASM)
include(SymbianApp)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
symbian_add_executable(file_io_contract probe.cpp)
target_compile_features(file_io_contract PRIVATE cxx_std_17)
target_compile_options(file_io_contract PRIVATE -marm -fno-rtti)
target_include_directories(file_io_contract PRIVATE "${SYMBIAN_SDK_PREFIX}/lib/armv6/mbedtls/include")
target_link_libraries(file_io_contract PRIVATE SymbianNativeAbi Symbian::EUser
 "${SYMBIAN_SDK_PREFIX}/proxies/efsrv/efsrv.dso"
 "${SYMBIAN_SDK_PREFIX}/proxies/libc/libc.dso"
 "${SYMBIAN_SDK_PREFIX}/lib/armv6/mbedtls/lib/libmbedcrypto.a")
symbian_publish_executable(file_io_contract UID3 0xe0000935 CAPABILITIES 0)
