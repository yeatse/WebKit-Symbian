# The browser: a native AVKON application around the Symbian port's WebView,
# linked through the SDK image flow and converted to an E32 EXE.
#
#   -DWEBKIT_SOURCE_DIR=…    WebKit checkout (for WebKitSymbian.h)
#   -DWEBKIT_BUILD_DIR=…     PORT=Symbian build with WebKitLegacy
#   -DDEPS_DIR=…             scripts/build-deps.sh's install directory
#   -DICU_LIBRARY_DIR=…      cross-built ICU libraries
#   -DBROWSER_STUB_ENGINE=ON a stand-in engine instead, to try the shell alone
cmake_minimum_required(VERSION 3.28)
project(browser_e32 LANGUAGES C CXX ASM)
include(SymbianApp)

set(WEBKIT_SOURCE_DIR "" CACHE PATH "WebKit source checkout")
set(WEBKIT_BUILD_DIR "" CACHE PATH "PORT=Symbian build directory")
set(DEPS_DIR "" CACHE PATH "Installed WebCore dependencies")
set(ICU_LIBRARY_DIR "" CACHE PATH "Cross-built ICU libraries")
set(BROWSER_UID3 0xE1A5C002 CACHE STRING "UID3 of browser.exe (unprotected range)")
option(BROWSER_STUB_ENGINE "Link the stand-in engine instead of WebKit" OFF)

set(shell_sources
  BrowserApp.cpp
  BrowserLog.cpp
  NetworkConnection.cpp
  PageList.cpp
  RunLoopDriver.cpp
  WebContainer.cpp)

if (BROWSER_STUB_ENGINE)
  symbian_add_executable(browser SOURCES ${shell_sources} StubWebView.cpp)
else ()
  symbian_add_executable(browser SOURCES ${shell_sources})
endif ()

target_include_directories(browser PRIVATE "${WEBKIT_SOURCE_DIR}/Source/WebKitLegacy/symbian")
target_compile_features(browser PRIVATE cxx_std_17)
# Leaves (exceptions) cross the AVKON framework, but no typeinfo: the FEP
# interfaces' type information lives in fepbase.dll, which has no proxy.
symbian_enable_native_leaves(browser SOURCES ${shell_sources})
set_property(SOURCE ${shell_sources} APPEND PROPERTY COMPILE_OPTIONS -fno-rtti)

# The SDK's Symbian::Avkon target is withheld because AknUtils.h needs a
# generated avkon.rsg; the shell does not include it, so it takes the native
# headers and ABI settings and names the import proxies itself.
target_link_libraries(browser PRIVATE SymbianNativeAbi Symbian::EUser)
# Stand-ins for two generated AVKON headers the SDK lacks (see include/).
target_include_directories(browser SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
# The original Nokia headers use redundant member qualifications. Microsoft
# extensions make __wchar_t a keyword, which Open C's sys/_types.h declares.
target_compile_options(browser PRIVATE -fms-extensions -Wno-microsoft-extra-qualification)
target_compile_definitions(browser PRIVATE __wchar_t=__symbian_wchar_t)
foreach (dll avkon cone eikcore eikcoctl eikctl apparc apgrfx bafl efsrv fbscli bitgdi gdi ws32 etext form esock)
  target_link_libraries(browser PRIVATE "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
endforeach ()

if (NOT BROWSER_STUB_ENGINE)
  set(L "${WEBKIT_BUILD_DIR}/lib")
  set(D "${DEPS_DIR}/lib")
  set(M "${SYMBIAN_SDK_PREFIX}/lib/armv6/mbedtls/lib")
  set(S "${SYMBIAN_SDK_PREFIX}/lib/armv6")
  target_link_libraries(browser PRIVATE
    --start-group
    "${L}/libWebKitLegacy.a"
    "${L}/libWebCore.a"
    "${L}/libPAL.a"
    "${L}/libJavaScriptCore.a"
    "${L}/libWTF.a"
    "${D}/libcairo.a" "${D}/libpixman-1.a" "${D}/libharfbuzz-icu.a" "${D}/libharfbuzz.a"
    "${D}/libfontconfig.a" "${D}/libxslt.a" "${D}/libxml2.a" "${D}/libsqlite3.a" "${D}/libpsl.a"
    "${D}/libwebpdemux.a" "${D}/libwebp.a" "${D}/libcurl.a" "${D}/libmbedtls_fs.a"
    "${M}/libmbedtls.a" "${M}/libmbedx509.a" "${M}/libmbedcrypto.a"
    "${ICU_LIBRARY_DIR}/libicui18n.a" "${ICU_LIBRARY_DIR}/libicuuc.a" "${ICU_LIBRARY_DIR}/libicudata.a"
    "${S}/libsymbian_portable_freetype.a" "${S}/libsymbian_portable_png.a"
    "${S}/libsymbian_portable_jpeg.a" "${S}/libsymbian_portable_zlib.a"
    --end-group
    Symbian::Streams
    "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libdl/libdl.dso")
  # Calls beyond an ARM BL's +-32 MB (or a Thumb BL's +-4 MB) go through
  # position-independent LLD thunks, which need no relocation and which the
  # E32 converter follows to the PLT (patches/symbian-native/0002).
  target_link_options(browser PRIVATE -znocopyreloc -znotext --icf=all --pic-veneer)
endif ()

# The SDK replaces an exception-enabled executable's layout with its own
# exe_unwind_image.ld once the target is complete; put the browser's layout,
# which keeps the same unwind sections, back afterwards.
function(browser_use_image_layout target)
  get_target_property(script ${target} SYMBIAN_IMAGE_LAYOUT)
  get_target_property(options ${target} LINK_OPTIONS)
  set(layout "${CMAKE_CURRENT_SOURCE_DIR}/browser_image.ld")
  list(TRANSFORM options REPLACE "^${script}$" "${layout}")
  set_target_properties(${target} PROPERTIES LINK_OPTIONS "${options}"
    LINK_DEPENDS "${layout}" SYMBIAN_IMAGE_LAYOUT "${layout}")
endfunction()
if (NOT BROWSER_STUB_ENGINE)
  cmake_language(DEFER CALL browser_use_image_layout browser)
endif ()

# NetworkServices (bit 13) for Open C's sockets.
symbian_publish_executable(browser UID3 ${BROWSER_UID3} CAPABILITIES 0x2000)

# Registration and caption resources, as the SDK's packager compiles them:
# preprocess as C, then rcomp.
foreach (resource browser_reg browser_loc)
  set(rss "${CMAKE_CURRENT_SOURCE_DIR}/${resource}.rss")
  set(rpp "${CMAKE_CURRENT_BINARY_DIR}/${resource}.rpp")
  set(rsc "${CMAKE_CURRENT_BINARY_DIR}/e32/${resource}.rsc")
  add_custom_command(OUTPUT "${rsc}"
    COMMAND "${CMAKE_C_COMPILER}" -E -P -x c -I "${SYMBIAN_SDK_PREFIX}/include/platform" "${rss}" -o "${rpp}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/e32"
    COMMAND "${SYMBIAN_SDK_PREFIX}/bin/rcomp" -u "-s${rpp}" "-o${rsc}" "-h${CMAKE_CURRENT_BINARY_DIR}/${resource}.rsg"
    DEPENDS "${rss}" VERBATIM)
  list(APPEND resources "${rsc}")
endforeach ()
add_custom_target(browser_resources ALL DEPENDS ${resources})
