cmake_minimum_required(VERSION 3.28)
project(arm11_exec_probe LANGUAGES CXX ASM)
include(SymbianApp)

get_filename_component(WEBKIT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
symbian_add_executable(arm11_exec_probe exec-probe.cpp)
target_sources(arm11_exec_probe PRIVATE
    "${WEBKIT_ROOT}/Source/WTF/wtf/symbian/SymbianSystem.cpp")
target_include_directories(arm11_exec_probe PRIVATE
    "${WEBKIT_ROOT}/Source/WTF/wtf/symbian")
target_link_libraries(arm11_exec_probe PRIVATE Symbian::Pthread Symbian::Math)
symbian_publish_executable(arm11_exec_probe UID3 0xe00008f2 CAPABILITIES 0x2000)
