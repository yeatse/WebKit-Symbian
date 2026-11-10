cmake_minimum_required(VERSION 3.28)
project(symbian_network_state_probe LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_executable(network_state_probe probe.cpp bridge.cpp)
symbian_enable_native_leaves(network_state_probe SOURCES probe.cpp bridge.cpp)
target_compile_features(network_state_probe PRIVATE cxx_std_23)
target_compile_options(network_state_probe PRIVATE -fno-rtti)
target_include_directories(network_state_probe PRIVATE official)
target_link_libraries(network_state_probe PRIVATE SymbianNativeAbi Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/esock/esock.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/connmon/connmon.dso")
symbian_publish_executable(network_state_probe UID3 0xe00008fb CAPABILITIES 0x2000)
