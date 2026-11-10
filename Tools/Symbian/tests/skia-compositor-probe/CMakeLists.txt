cmake_minimum_required(VERSION 3.28)
project(symbian_skia_compositor_probe LANGUAGES CXX ASM)
include(SymbianApp)
get_filename_component(probe_checkout "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(WEBKIT_SOURCE_DIR "${probe_checkout}" CACHE PATH "WebKit checkout")
set(WEBKIT_BUILD_DIR "" CACHE PATH "Configured Symbian WebKit build")
set(DEPS_DIR "" CACHE PATH "Installed WebKit dependencies")
set(ICU_LIBRARY_DIR "" CACHE PATH "Installed ICU libraries")
set(JPEG_LIBRARY "${DEPS_DIR}/lib/libjpeg.a" CACHE FILEPATH "libjpeg-turbo library")

symbian_add_executable(skia_compositor_probe probe.cpp)
target_compile_features(skia_compositor_probe PRIVATE cxx_std_23)
target_compile_options(skia_compositor_probe PRIVATE -marm -mfpu=vfpv2 -mfloat-abi=softfp
    -fdeclspec -Wno-ignored-attributes -fno-rtti)
target_compile_definitions(skia_compositor_probe PRIVATE BUILDING_WITH_CMAKE=1 HAVE_CONFIG_H=1
    SK_GANESH SK_GL SK_BUILD_FOR_UNIX SK_R32_SHIFT=16 SK_RELEASE SK_ENABLE_OPTIMIZE_SIZE
    SK_ENABLE_PRECOMPILE SK_ASSUME_GL=0 SK_ASSUME_GL_ES=1 SK_ASSUME_WEBGL=0
    SK_TRIVIAL_ABI=[[clang::trivial_abi]])
target_include_directories(skia_compositor_probe PRIVATE
    "${WEBKIT_BUILD_DIR}"
    "${WEBKIT_SOURCE_DIR}/Source/WebCore"
    "${WEBKIT_SOURCE_DIR}/Source/WebCore/platform/symbian"
    "${WEBKIT_SOURCE_DIR}/Source/ThirdParty/ANGLE/include"
    "${WEBKIT_SOURCE_DIR}/Source/ThirdParty/skia"
    "${WEBKIT_BUILD_DIR}/WebCore/DerivedSources"
    "${WEBKIT_BUILD_DIR}/WebCore/PrivateHeaders"
    "${WEBKIT_BUILD_DIR}/WebCore/PrivateHeaders/WebCore"
    "${WEBKIT_BUILD_DIR}/JavaScriptCore/Headers"
    "${WEBKIT_BUILD_DIR}/JavaScriptCore/PrivateHeaders"
    "${WEBKIT_BUILD_DIR}/WTF/Headers"
    "${WEBKIT_BUILD_DIR}/PAL/Headers"
    "${WEBKIT_BUILD_DIR}/bmalloc/Headers"
    "${WEBKIT_BUILD_DIR}/Skia/Headers/top"
    "${WEBKIT_BUILD_DIR}/Skia/Headers/mod")
target_include_directories(skia_compositor_probe SYSTEM PRIVATE
    "${SYMBIAN_SDK_PREFIX}/include/abseil" "${DEPS_DIR}/include"
    "${DEPS_DIR}/include/harfbuzz"
    "${ICU_LIBRARY_DIR}/../include")

set(L "${WEBKIT_BUILD_DIR}/lib")
set(D "${DEPS_DIR}/lib")
set(S "${SYMBIAN_SDK_PREFIX}/lib/armv6")
set(M "${S}/mbedtls/lib")
target_link_libraries(skia_compositor_probe PRIVATE --start-group
    "${L}/libWebCore.a" "${L}/libPAL.a" "${L}/libJavaScriptCore.a"
    "${L}/libWTF.a" "${L}/libbmalloc.a" "${L}/libSymbianEGLBridge.a"
    "${L}/libGLESv2.a" "${L}/libANGLE.a" "${L}/libSkia.a"
    "${D}/libharfbuzz-icu.a" "${D}/libharfbuzz.a" "${D}/libfontconfig.a"
    "${D}/libxslt.a" "${D}/libxml2.a" "${D}/libsqlite3.a" "${D}/libpsl.a"
    "${D}/libwebpdemux.a" "${D}/libwebp.a" "${D}/libcurl.a"
    "${D}/libnghttp2.a" "${D}/libmbedtls_fs.a"
    "${D}/libwoff2dec.a" "${D}/libwoff2common.a" "${D}/libbrotlidec.a" "${D}/libbrotlicommon.a"
    "${M}/libmbedtls.a" "${M}/libmbedx509.a" "${M}/libmbedcrypto.a"
    "${ICU_LIBRARY_DIR}/libicui18n.a" "${ICU_LIBRARY_DIR}/libicuuc.a" "${ICU_LIBRARY_DIR}/libicudata.a"
    "${S}/libsymbian_portable_freetype.a" "${S}/libsymbian_portable_png.a"
    "${JPEG_LIBRARY}" "${S}/libsymbian_portable_zlib.a"
    --end-group Symbian::Streams Symbian::EUser
    "${SYMBIAN_SDK_PREFIX}/proxies/libegl/libegl.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libglesv2/libglesv2.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libdl/libdl.dso")
foreach(dll esock insock connmon imageconversion fbscli efsrv ws32 hal mediaclientaudiostream)
    target_link_libraries(skia_compositor_probe PRIVATE
        "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
endforeach()
target_link_options(skia_compositor_probe PRIVATE -znocopyreloc -znotext --icf=all --pic-veneer)
symbian_publish_executable(skia_compositor_probe UID3 0xe00008fa CAPABILITIES 0)
