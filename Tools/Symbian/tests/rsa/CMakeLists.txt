cmake_minimum_required(VERSION 3.28)
project(symbian_rsa_probe LANGUAGES CXX ASM)
include(SymbianApp)
get_filename_component(probe_checkout "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(WEBKIT_BUILD "" CACHE PATH "Existing WebKit build with the real Symbian entropy bridge")
if(NOT EXISTS "${WEBKIT_BUILD}/lib/libWTF.a")
    message(FATAL_ERROR "WEBKIT_BUILD must contain the existing production libWTF.a")
endif()
find_package(MbedTLS CONFIG REQUIRED PATHS "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/mbedtls/lib/cmake/MbedTLS" NO_DEFAULT_PATH)
symbian_add_executable(rsa_probe host.cpp)
target_compile_features(rsa_probe PRIVATE cxx_std_23)
target_compile_options(rsa_probe PRIVATE -marm)
target_compile_definitions(rsa_probe PRIVATE RSA_NATIVE_PROBE=1)
target_include_directories(rsa_probe PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    "${probe_checkout}/Source/WebCore/crypto/mbedtls"
    "${probe_checkout}/Source/WTF")
target_link_libraries(rsa_probe PRIVATE "${WEBKIT_BUILD}/lib/libWTF.a" MbedTLS::mbedcrypto Symbian::Pthread
    "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso")
symbian_publish_executable(rsa_probe UID3 0xe0000902 CAPABILITIES 0)
