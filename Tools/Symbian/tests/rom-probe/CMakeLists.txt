cmake_minimum_required(VERSION 3.28)
project(rom_probe LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_executable(romprobe probe.cpp)
target_compile_options(romprobe PRIVATE -marm -O2)
target_link_libraries(romprobe PRIVATE Symbian::EUser Symbian::Pthread
    "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso")
symbian_publish_executable(romprobe UID3 0xE1A5C3B5 CAPABILITIES 0)
