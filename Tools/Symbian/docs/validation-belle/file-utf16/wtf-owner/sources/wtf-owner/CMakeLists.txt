cmake_minimum_required(VERSION 3.28)
project(symbian_wtf_file_owner_contract LANGUAGES CXX ASM)
include(SymbianApp)

set(WEBKIT_SOURCE_DIR "/workspace/WebKit-Symbian-graphics" CACHE PATH "WebKit checkout")
set(WEBKIT_BUILD_DIR "/workspace/symbian-graphics-work/build/webcore" CACHE PATH "Matching WebKit build")
set(ICU_LIBRARY_DIR "/workspace/symbian-work/icu/install/lib" CACHE PATH "Matching ARM ICU archives")
set(ICU_INCLUDE_DIR "/workspace/symbian-work/icu/install/include" CACHE PATH "Matching ARM ICU headers")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

symbian_add_executable(wtf_file_owner_contract worker.cpp)
target_compile_features(wtf_file_owner_contract PRIVATE cxx_std_23)
target_compile_definitions(wtf_file_owner_contract PRIVATE
  BUILDING_WEBKIT=1 BUILDING_WITH_CMAKE=1 HAVE_CONFIG_H=1)
target_include_directories(wtf_file_owner_contract PRIVATE
  "${WEBKIT_BUILD_DIR}"
  "${WEBKIT_BUILD_DIR}/WTF/DerivedSources"
  "${WEBKIT_SOURCE_DIR}/Source/WTF"
  "${WEBKIT_SOURCE_DIR}/Source/WTF/wtf"
  "${SYMBIAN_SDK_PREFIX}/lib/armv6/mbedtls/include")
target_include_directories(wtf_file_owner_contract SYSTEM PRIVATE "${ICU_INCLUDE_DIR}")
# Match the engine's A32/VFPv2 configuration while retaining the base AAPCS ABI.
target_compile_options(wtf_file_owner_contract PRIVATE
  -marm -mfloat-abi=softfp -mfpu=vfpv2 -fno-rtti -fno-strict-aliasing)
target_link_libraries(wtf_file_owner_contract PRIVATE
  SymbianNativeAbi
  --start-group
  "${WEBKIT_BUILD_DIR}/lib/libWTF.a"
  "${ICU_LIBRARY_DIR}/libicui18n.a"
  "${ICU_LIBRARY_DIR}/libicuuc.a"
  "${ICU_LIBRARY_DIR}/libicudata.a"
  "${SYMBIAN_SDK_PREFIX}/lib/armv6/mbedtls/lib/libmbedcrypto.a"
  --end-group
  Symbian::Streams Symbian::EUser
  "${SYMBIAN_SDK_PREFIX}/proxies/efsrv/efsrv.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/libdl/libdl.dso")
target_link_options(wtf_file_owner_contract PRIVATE -znocopyreloc --pic-veneer)
symbian_publish_executable(wtf_file_owner_contract UID3 0xe0000936 CAPABILITIES 0)
