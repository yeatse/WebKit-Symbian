cmake_minimum_required(VERSION 3.28)
project(symbian_native_clipboard LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_executable(clipboard_probe probe.cpp)
symbian_enable_native_leaves(clipboard_probe SOURCES probe.cpp)
target_compile_features(clipboard_probe PRIVATE cxx_std_17)
target_compile_options(clipboard_probe PRIVATE -marm)
target_include_directories(clipboard_probe SYSTEM PRIVATE "${SYMBIAN_SDK_PREFIX}/include/native")
target_link_libraries(clipboard_probe PRIVATE Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/efsrv/efsrv.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/bafl/bafl.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/etext/etext.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/estor/estor.dso")
symbian_publish_executable(clipboard_probe UID3 0xe0000901 CAPABILITIES 0)
