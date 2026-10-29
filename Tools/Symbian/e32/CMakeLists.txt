# Re-links the JSCOnly build's jsc shell through the SDK image flow (SDK
# startup, import proxies, symbian-native) and converts it to an E32 EXE.
cmake_minimum_required(VERSION 3.28)
project(jsc_e32 LANGUAGES C CXX ASM)
include(SymbianApp)

set(WEBKIT_BUILD_DIR "" CACHE PATH "JSCOnly build directory")
set(ICU_LIBRARY_DIR "" CACHE PATH "Cross-built ICU libraries")
set(JSC_UID3 0xE1A5C001 CACHE STRING "UID3 of jsc.exe (unprotected range)")

symbian_add_executable(jsc SOURCES empty.c
  LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/jsc_image.ld")
# ICU is built with RTTI: its typeinfo objects in .rodata point at
# drtaeabi.dll's __cxxabiv1 vtables. Keep those as read-only import
# relocations for the converter instead of R_ARM_COPY data.
target_link_options(jsc PRIVATE -znocopyreloc -znotext)
# Position-independent range-extension thunks, as for the browser.
target_link_options(jsc PRIVATE --pic-veneer)
target_link_libraries(jsc PRIVATE
  "${WEBKIT_BUILD_DIR}/Source/JavaScriptCore/shell/CMakeFiles/jsc.dir/__/jsc.cpp.o"
  --start-group
  "${WEBKIT_BUILD_DIR}/lib/libJavaScriptCore.a"
  "${WEBKIT_BUILD_DIR}/lib/libWTF.a"
  "${ICU_LIBRARY_DIR}/libicui18n.a"
  "${ICU_LIBRARY_DIR}/libicuuc.a"
  "${ICU_LIBRARY_DIR}/libicudata.a"
  --end-group
  Symbian::Streams
  Symbian::Crypto
  "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/libdl/libdl.dso")
symbian_publish_executable(jsc UID3 ${JSC_UID3})
