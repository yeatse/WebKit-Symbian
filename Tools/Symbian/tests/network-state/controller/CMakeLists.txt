cmake_minimum_required(VERSION 3.28)
project(symbian_network_state_controller LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_executable(network_state_controller controller.cpp)
symbian_enable_native_leaves(network_state_controller SOURCES controller.cpp)
target_compile_features(network_state_controller PRIVATE cxx_std_23)
target_include_directories(network_state_controller PRIVATE ../native/official)
target_link_libraries(network_state_controller PRIVATE SymbianNativeAbi Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/esock/esock.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/connmon/connmon.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/ws32/ws32.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/apgrfx/apgrfx.dso")
# Test-only interface controller: NetworkServices and NetworkControl.
# Requires converter patch 0005; the browser retains NetworkServices only.
symbian_publish_executable(network_state_controller UID3 0xe00008fc CAPABILITIES 0x2400)
