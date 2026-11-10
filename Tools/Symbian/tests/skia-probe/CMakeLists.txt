cmake_minimum_required(VERSION 3.28)
project(symbian_skia_probe LANGUAGES CXX ASM)
include(SymbianApp)
get_filename_component(probe_checkout "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(WEBKIT_SOURCE_DIR "${probe_checkout}" CACHE PATH "WebKit checkout")
set(WEBKIT_BUILD_DIR "" CACHE PATH "Configured WebKit build containing libSkia.a")
set(DEPS_DIR "" CACHE PATH "Installed WebKit dependencies")
set(JPEG_LIBRARY "${DEPS_DIR}/lib/libjpeg.a" CACHE FILEPATH "libjpeg-turbo library")
symbian_add_executable(skia_probe probe.cpp)
target_compile_features(skia_probe PRIVATE cxx_std_23)
target_compile_options(skia_probe PRIVATE -marm -mfpu=vfpv2 -mfloat-abi=softfp)
target_compile_definitions(skia_probe PRIVATE
    SK_BUILD_FOR_UNIX SK_R32_SHIFT=16 SK_RELEASE SK_TRIVIAL_ABI=[[clang::trivial_abi]])
target_include_directories(skia_probe PRIVATE "${WEBKIT_SOURCE_DIR}/Source/ThirdParty/skia")
set(sdk_libraries "${SYMBIAN_SDK_PREFIX}/lib/armv6")
target_link_libraries(skia_probe PRIVATE --start-group
    "${WEBKIT_BUILD_DIR}/lib/libSkia.a" "${WEBKIT_BUILD_DIR}/lib/libWTF.a"
    "${WEBKIT_BUILD_DIR}/lib/libbmalloc.a" "${DEPS_DIR}/lib/libfontconfig.a"
    "${DEPS_DIR}/lib/libxml2.a" "${JPEG_LIBRARY}"
    "${sdk_libraries}/libsymbian_portable_freetype.a"
    "${sdk_libraries}/libsymbian_portable_png.a"
    "${sdk_libraries}/libsymbian_portable_zlib.a"
    --end-group Symbian::Streams Symbian::EUser)
symbian_publish_executable(skia_probe UID3 0xe00008f8 CAPABILITIES 0)
