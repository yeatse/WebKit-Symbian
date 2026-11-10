cmake_minimum_required(VERSION 3.28)
project(symbian_clipboard_adapter_probe LANGUAGES CXX ASM)
include(SymbianApp)
get_filename_component(probe_checkout "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
symbian_add_executable(clipboard_adapter_probe probe.cpp production.cpp)
symbian_enable_native_leaves(clipboard_adapter_probe SOURCES probe.cpp production.cpp)
target_compile_features(clipboard_adapter_probe PRIVATE cxx_std_17)
target_compile_options(clipboard_adapter_probe PRIVATE -marm)
target_include_directories(clipboard_adapter_probe PRIVATE
    "${probe_checkout}/Tools/Symbian/browser" "${probe_checkout}/Source/WebKitLegacy/symbian")
target_include_directories(clipboard_adapter_probe SYSTEM PRIVATE "${SYMBIAN_SDK_PREFIX}/include/native")
target_link_libraries(clipboard_adapter_probe PRIVATE Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/efsrv/efsrv.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/bafl/bafl.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/etext/etext.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/estor/estor.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso")
symbian_publish_executable(clipboard_adapter_probe UID3 0xe0000903 CAPABILITIES 0)
