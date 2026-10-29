cmake_minimum_required(VERSION 3.28)
project(symbian_egl_probe LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_executable(egl_probe probe.cpp)
target_compile_options(egl_probe PRIVATE -marm)
target_link_libraries(egl_probe PRIVATE Symbian::EGL Symbian::GLES2
    Symbian::WindowServer Symbian::Gdi Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/fbscli/fbscli.dso")
symbian_publish_executable(egl_probe UID3 0xe00008f3 CAPABILITIES 0)
