cmake_minimum_required(VERSION 3.28)
project(symbian_mmf_probe LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_executable(mmf_probe probe.cpp)
symbian_enable_native_leaves(mmf_probe SOURCES probe.cpp)
target_compile_options(mmf_probe PRIVATE -marm)
target_link_libraries(mmf_probe PRIVATE Symbian::Video Symbian::Audio
    Symbian::WindowServer Symbian::Gdi Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso")
# NetworkServices permits the URL mode; the probe does not request recording.
symbian_publish_executable(mmf_probe UID3 0xe00008f4 CAPABILITIES 0x2000)
