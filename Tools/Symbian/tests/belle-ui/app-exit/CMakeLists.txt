cmake_minimum_required(VERSION 3.28)
project(symbian_browser_exit_request LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_executable(browser_exit_request request.cpp)
symbian_enable_native_leaves(browser_exit_request SOURCES request.cpp)
target_compile_options(browser_exit_request PRIVATE -marm)
target_link_libraries(browser_exit_request PRIVATE SymbianNativeAbi Symbian::WindowServer Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/apgrfx/apgrfx.dso")
symbian_publish_executable(browser_exit_request UID3 0xe00008fa CAPABILITIES 0)
