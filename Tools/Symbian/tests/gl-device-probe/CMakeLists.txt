cmake_minimum_required(VERSION 3.28)
project(symbian_gl_device_probe LANGUAGES C CXX ASM)
include(SymbianApp)
get_filename_component(probe_checkout "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(WEBKIT_SOURCE_DIR "${probe_checkout}" CACHE PATH "WebKit checkout")
set(WEBKIT_BUILD_DIR "" CACHE PATH "Configured WebKit build containing libSkia.a")
set(DEPS_DIR "" CACHE PATH "Installed WebKit dependencies")
set(JPEG_LIBRARY "${DEPS_DIR}/lib/libjpeg.a" CACHE FILEPATH "libjpeg-turbo library")
set(skia "${WEBKIT_SOURCE_DIR}/Source/ThirdParty/skia")

# Skia's own interface validation, as a function that reports the line of
# the requirement that fails.
file(READ "${skia}/src/gpu/ganesh/gl/GrGLInterfaceAutogen.cpp" autogen)
string(FIND "${autogen}" "bool GrGLInterface::validate() const {" validate_begin)
string(FIND "${autogen}" "#if defined(GPU_TEST_UTILS)" validate_end REVERSE)
if (validate_begin LESS 0 OR validate_end LESS validate_begin)
  message(FATAL_ERROR "GrGLInterface::validate() is not where the probe expects it")
endif ()
math(EXPR validate_length "${validate_end} - ${validate_begin}")
string(SUBSTRING "${autogen}" ${validate_begin} ${validate_length} validate)
string(REPLACE "GrGLInterface::validate" "ProbeInterface::probeValidate" validate "${validate}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/validate.inc" "${validate}")

symbian_add_executable(glprobe probe.cpp window.cpp images.cpp)
target_compile_features(glprobe PRIVATE cxx_std_23)
target_compile_options(glprobe PRIVATE -marm -mfpu=vfpv2 -mfloat-abi=softfp
    -fdeclspec -Wno-ignored-attributes)
target_compile_definitions(glprobe PRIVATE SK_GANESH SK_GL
    SK_BUILD_FOR_UNIX SK_R32_SHIFT=16 SK_RELEASE SK_ENABLE_OPTIMIZE_SIZE
    SK_ENABLE_PRECOMPILE SK_ASSUME_GL=0 SK_ASSUME_GL_ES=1 SK_ASSUME_WEBGL=0
    SK_TRIVIAL_ABI=[[clang::trivial_abi]])
target_include_directories(glprobe PRIVATE "${skia}" "${CMAKE_CURRENT_BINARY_DIR}")
# The surface manager headers; after the platform headers, which they duplicate in part.
set_source_files_properties(images.cpp PROPERTIES COMPILE_OPTIONS
    "-idirafter;${SYMBIAN_SDK_PREFIX}/include/native")
target_include_directories(glprobe SYSTEM PRIVATE
    "${SYMBIAN_SDK_PREFIX}/include/graphics" "${DEPS_DIR}/include")
set(sdk_libraries "${SYMBIAN_SDK_PREFIX}/lib/armv6")
target_link_libraries(glprobe PRIVATE --start-group
    "${WEBKIT_BUILD_DIR}/lib/libSkia.a" "${WEBKIT_BUILD_DIR}/lib/libWTF.a"
    "${WEBKIT_BUILD_DIR}/lib/libbmalloc.a" "${DEPS_DIR}/lib/libfontconfig.a"
    "${DEPS_DIR}/lib/libxml2.a" "${JPEG_LIBRARY}"
    "${sdk_libraries}/libsymbian_portable_freetype.a"
    "${sdk_libraries}/libsymbian_portable_png.a"
    "${sdk_libraries}/libsymbian_portable_zlib.a"
    --end-group Symbian::Streams Symbian::WindowServer Symbian::Gdi Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/fbscli/fbscli.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libegl/libegl.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libglesv2/libglesv2.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/surfacemanager/surfacemanager.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/surfaceupdateclient/surfaceupdateclient.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso")
symbian_publish_executable(glprobe UID3 0xE1A5C0F1 CAPABILITIES 0)

set(rpp "${CMAKE_CURRENT_BINARY_DIR}/glprobe_reg.rpp")
set(rsc "${CMAKE_CURRENT_BINARY_DIR}/e32/glprobe_reg.rsc")
add_custom_command(OUTPUT "${rsc}"
  COMMAND "${CMAKE_C_COMPILER}" -E -P -x c
    -I "${SYMBIAN_SDK_PREFIX}/include/platform" -I "${SYMBIAN_SDK_PREFIX}/include/native"
    "${CMAKE_CURRENT_SOURCE_DIR}/glprobe_reg.rss" -o "${rpp}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/e32"
  COMMAND "${SYMBIAN_SDK_PREFIX}/bin/rcomp" -u "-s${rpp}" "-o${rsc}" "-h${CMAKE_CURRENT_BINARY_DIR}/glprobe_reg.rsg"
  DEPENDS glprobe_reg.rss VERBATIM)
add_custom_target(glprobe_resources ALL DEPENDS "${rsc}")
