cmake_minimum_required(VERSION 3.28)
project(symbian_aligned_pressure LANGUAGES CXX ASM)
include(SymbianApp)
get_filename_component(probe_checkout "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(WEBKIT_SOURCE_DIR "${probe_checkout}" CACHE PATH "WebKit checkout")
symbian_add_executable(aligned_pressure probe.cpp bridge.cpp)
target_compile_features(aligned_pressure PRIVATE cxx_std_23)
target_compile_options(aligned_pressure PRIVATE -marm)
target_include_directories(aligned_pressure PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    "${WEBKIT_SOURCE_DIR}/Source/WTF/wtf/symbian")
target_link_libraries(aligned_pressure PRIVATE Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libpthread/libpthread.dso")
symbian_publish_executable(aligned_pressure UID3 0xe00008f6 CAPABILITIES 0)
