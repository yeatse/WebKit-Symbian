cmake_minimum_required(VERSION 3.16)
project(SymbianWOFF2 LANGUAGES C CXX)

set(BROTLI_SOURCE_DIR "" CACHE PATH "Verified Brotli 1.2.0 release")
set(WOFF2_SOURCE_DIR "" CACHE PATH "Verified WOFF2 1.0.2 release")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Static decoder libraries" FORCE)
set(BROTLI_BUILD_TOOLS OFF CACHE BOOL "No guest command-line tools" FORCE)
set(BROTLI_DISABLE_TESTS ON CACHE BOOL "Host/guest tests are separate" FORCE)
set(BROTLI_BUNDLED_MODE ON)
add_subdirectory("${BROTLI_SOURCE_DIR}" brotli EXCLUDE_FROM_ALL)

# Upstream decoder/common source lists. No encoder, utilities, fuzzers or
# replacement font engine is built into the browser.
add_library(woff2common STATIC
    "${WOFF2_SOURCE_DIR}/src/table_tags.cc"
    "${WOFF2_SOURCE_DIR}/src/variable_length.cc"
    "${WOFF2_SOURCE_DIR}/src/woff2_common.cc")
add_library(woff2dec STATIC
    "${WOFF2_SOURCE_DIR}/src/woff2_dec.cc"
    "${WOFF2_SOURCE_DIR}/src/woff2_out.cc")
foreach(target woff2common woff2dec)
    target_include_directories(${target} PUBLIC "${WOFF2_SOURCE_DIR}/include"
        PRIVATE "${WOFF2_SOURCE_DIR}/src")
    target_compile_definitions(${target} PRIVATE __STDC_FORMAT_MACROS)
    # Match WebCore's C++ runtime contract; decode failures use bool returns.
    target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 23 POSITION_INDEPENDENT_CODE ON)
endforeach()
target_link_libraries(woff2dec PUBLIC woff2common brotlidec)
install(TARGETS woff2common woff2dec brotlicommon brotlidec ARCHIVE DESTINATION lib)
install(DIRECTORY "${WOFF2_SOURCE_DIR}/include/woff2" "${BROTLI_SOURCE_DIR}/c/include/brotli" DESTINATION include)
install(FILES "${WOFF2_SOURCE_DIR}/LICENSE" DESTINATION share/licenses/woff2)
install(FILES "${BROTLI_SOURCE_DIR}/LICENSE" DESTINATION share/licenses/brotli)

foreach(name woff2common woff2dec brotlicommon brotlidec)
    if(name MATCHES "^woff2")
        set(version 1.0.2)
    else()
        set(version 1.2.0)
    endif()
    set(requires "")
    if(name STREQUAL "woff2dec")
        set(requires "libwoff2common libbrotlidec")
    elseif(name STREQUAL "brotlidec")
        set(requires "libbrotlicommon")
    endif()
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/lib${name}.pc" CONTENT
"prefix=${CMAKE_INSTALL_PREFIX}
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: lib${name}
Description: Static Symbian ${name}
Version: ${version}
Requires.private: ${requires}
Libs: -L\${libdir} -l${name}
Cflags: -I\${includedir}
")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/lib${name}.pc" DESTINATION lib/pkgconfig)
endforeach()
