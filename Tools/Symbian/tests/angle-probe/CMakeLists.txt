cmake_minimum_required(VERSION 3.28)
project(symbian_angle_probe LANGUAGES CXX ASM)
include(SymbianApp)
get_filename_component(probe_checkout "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(WEBKIT_SOURCE_DIR "${probe_checkout}" CACHE PATH "WebKit checkout")
set(ANGLE_BUILD_DIR "" CACHE PATH "ANGLE static build")
if(NOT EXISTS "${ANGLE_BUILD_DIR}/lib/libANGLE.a" OR NOT EXISTS "${ANGLE_BUILD_DIR}/lib/libGLESv2.a")
    message(FATAL_ERROR "ANGLE_BUILD_DIR must contain the built Symbian ANGLE and GLESv2 libraries")
endif()
symbian_add_executable(angle_probe probe.cpp)
target_compile_features(angle_probe PRIVATE cxx_std_23)
target_compile_options(angle_probe PRIVATE -marm -mfpu=vfpv2 -mfloat-abi=softfp)
target_compile_definitions(angle_probe PRIVATE EGL_NO_PLATFORM_SPECIFIC_TYPES EGL_EGL_PROTOTYPES=0 GL_GLES_PROTOTYPES=0)
target_include_directories(angle_probe BEFORE PRIVATE
    "${WEBKIT_SOURCE_DIR}/Source/ThirdParty/ANGLE/include"
    "${WEBKIT_SOURCE_DIR}/Source/ThirdParty/ANGLE/src")
target_link_libraries(angle_probe PRIVATE
    "${ANGLE_BUILD_DIR}/lib/libGLESv2.a" "${ANGLE_BUILD_DIR}/lib/libANGLE.a"
    "${SYMBIAN_SDK_PREFIX}/lib/armv6/libsymbian_portable_zlib.a"
    Symbian::Streams Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/libpthread/libpthread.dso")
symbian_publish_executable(angle_probe UID3 0xe00008f4 CAPABILITIES 0)
