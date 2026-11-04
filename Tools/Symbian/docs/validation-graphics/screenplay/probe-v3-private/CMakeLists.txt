cmake_minimum_required(VERSION 3.28)
project(symbian_screenplay_private_capability LANGUAGES CXX ASM)
include(SymbianApp)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SCREENPLAY_SDK_STAGE "" CACHE PATH "Reviewed external ScreenPlay headers/proxies")
if(NOT EXISTS "${SCREENPLAY_SDK_STAGE}/include/native/graphics/surfacemanager.h")
  message(FATAL_ERROR "Set SCREENPLAY_SDK_STAGE to the reviewed external stage")
endif()
symbian_add_executable(screenplay_private_capability screenplay_probe.cpp ScreenPlayUpdateSession.cpp)
symbian_enable_native_leaves(screenplay_private_capability SOURCES screenplay_probe.cpp)
target_compile_features(screenplay_private_capability PRIVATE cxx_std_17)
target_compile_options(screenplay_private_capability PRIVATE -marm -fno-rtti)
target_include_directories(screenplay_private_capability BEFORE PRIVATE
  "${SCREENPLAY_SDK_STAGE}/include/native")
target_link_libraries(screenplay_private_capability PRIVATE SymbianNativeAbi Symbian::EUser
  "${SCREENPLAY_SDK_STAGE}/proxies/surfacemanager/surfacemanager.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/ws32/ws32.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/fbscli/fbscli.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/bitgdi/bitgdi.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/gdi/gdi.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/libc/libc.dso")
symbian_publish_executable(screenplay_private_capability UID3 0xe0000939 CAPABILITIES 0)
