cmake_minimum_required(VERSION 3.28)
project(symbian_ganesh_probe LANGUAGES CXX ASM)
include(SymbianApp)
get_filename_component(probe_checkout "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(WEBKIT_SOURCE_DIR "${probe_checkout}" CACHE PATH "WebKit checkout")
set(WEBKIT_BUILD_DIR "" CACHE PATH "Configured WebKit build containing libSkia.a")
set(DEPS_DIR "" CACHE PATH "Installed WebKit dependencies")
set(JPEG_LIBRARY "${DEPS_DIR}/lib/libjpeg.a" CACHE FILEPATH "libjpeg-turbo library")
symbian_add_executable(ganesh_probe probe.cpp)
target_compile_features(ganesh_probe PRIVATE cxx_std_23)
target_compile_options(ganesh_probe PRIVATE -marm -mfpu=vfpv2 -mfloat-abi=softfp
    -fdeclspec -Wno-ignored-attributes)
target_compile_definitions(ganesh_probe PRIVATE SK_GANESH SK_GL
    SK_BUILD_FOR_UNIX SK_R32_SHIFT=16 SK_RELEASE SK_ENABLE_OPTIMIZE_SIZE
    SK_ENABLE_PRECOMPILE SK_ASSUME_GL=0 SK_ASSUME_GL_ES=1 SK_ASSUME_WEBGL=0
    SK_TRIVIAL_ABI=[[clang::trivial_abi]])
target_include_directories(ganesh_probe PRIVATE "${WEBKIT_SOURCE_DIR}/Source/ThirdParty/skia")
target_include_directories(ganesh_probe SYSTEM PRIVATE
    "${SYMBIAN_SDK_PREFIX}/include/graphics" "${DEPS_DIR}/include")
set(sdk_libraries "${SYMBIAN_SDK_PREFIX}/lib/armv6")
target_link_libraries(ganesh_probe PRIVATE --start-group
    "${WEBKIT_BUILD_DIR}/lib/libSkia.a" "${WEBKIT_BUILD_DIR}/lib/libWTF.a"
    "${WEBKIT_BUILD_DIR}/lib/libbmalloc.a" "${DEPS_DIR}/lib/libfontconfig.a"
    "${DEPS_DIR}/lib/libxml2.a" "${JPEG_LIBRARY}"
    "${sdk_libraries}/libsymbian_portable_freetype.a"
    "${sdk_libraries}/libsymbian_portable_png.a"
    "${sdk_libraries}/libsymbian_portable_zlib.a"
    --end-group Symbian::Streams Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/libegl/libegl.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libglesv2/libglesv2.dso")
symbian_publish_executable(ganesh_probe UID3 0xe00008f9 CAPABILITIES 0)
