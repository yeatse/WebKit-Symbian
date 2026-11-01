cmake_minimum_required(VERSION 3.28)
project(eka_avkonfep_cancel LANGUAGES CXX ASM)
include(SymbianApp)
set(FEP_SDK_OVERLAY "${CMAKE_CURRENT_SOURCE_DIR}/../sdk-overlay" CACHE PATH "Missing official FEP SDK header and proxy overlay")
set(FEP_IMPORT_PROXY "${FEP_SDK_OVERLAY}/proxies/fepbase/fepbase.dso")
set(FEP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/HostBridgedImeFEP.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/Log.cpp")
symbian_add_dynamic_library(avkonfep_general
  SOURCES ${FEP_SOURCES} Dispatch.S "${SYMBIAN_SDK_PREFIX}/cmake/exception_descriptor.S"
  LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/fep_unwind_image.ld"
  UID3 0x100056de EXPORT_DEFINITION avkonfep_general.def
  IMPORT_PROXIES "${FEP_IMPORT_PROXY}")
target_include_directories(avkonfep_general PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}" "${FEP_SDK_OVERLAY}/include/native")
target_compile_definitions(avkonfep_general PRIVATE EKA2=1)
target_compile_options(avkonfep_general PRIVATE -fms-extensions -Wno-microsoft-extra-qualification -marm
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/native-patch-compat.h>")
target_link_options(avkonfep_general PRIVATE "-Map=${CMAKE_CURRENT_BINARY_DIR}/avkonfep_general.elf.map")
target_link_libraries(avkonfep_general PRIVATE SymbianNativeAbi Symbian::EUser)
# Use only the already validated frozen proxies needed by this FEP TU. The
# broad Avkon facility is blocked by generated AknUtils resources, not used here.
foreach(dll cone avkon ecom eikcoctl eikctl form)
  target_link_libraries(avkonfep_general PRIVATE "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
endforeach()
symbian_enable_native_leaves(avkonfep_general SOURCES ${FEP_SOURCES})
