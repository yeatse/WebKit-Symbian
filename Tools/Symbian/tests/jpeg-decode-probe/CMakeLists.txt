cmake_minimum_required(VERSION 3.28)
project(jpeg_decode_probe LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_executable(jpegprobe probe.cpp)
symbian_enable_native_leaves(jpegprobe SOURCES probe.cpp)
target_compile_options(jpegprobe PRIVATE -marm)
target_include_directories(jpegprobe SYSTEM PRIVATE "${SYMBIAN_SDK_PREFIX}/include/native")
target_link_libraries(jpegprobe PRIVATE Symbian::PortableJpeg Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/efsrv/efsrv.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/fbscli/fbscli.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/ecom/ecom.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/imageconversion/imageconversion.dso")
symbian_publish_executable(jpegprobe UID3 0xE1A5C3B4 CAPABILITIES 0)
