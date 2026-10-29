include(platform/Adwaita.cmake)
include(platform/Cairo.cmake)
include(platform/Curl.cmake)
include(platform/FreeType.cmake)
include(platform/ImageDecoders.cmake)
include(platform/MbedTLS.cmake)
include(platform/TextureMapper.cmake)
include(platform/SymbianWebGL.cmake)
include(SymbianNativeGraphics)

# Compile the native EGL boundary without ANGLE's include directories.
add_library(SymbianEGLBridge STATIC platform/symbian/SymbianEGL.cpp)
target_include_directories(SymbianEGLBridge SYSTEM PRIVATE
    "${SYMBIAN_SDK_PREFIX}/include/graphics")
target_link_libraries(SymbianEGLBridge PRIVATE SymbianNative::EGL)

list(APPEND WebCore_SOURCES
    platform/graphics/PlatformDisplay.cpp
    platform/graphics/egl/GLContext.cpp
    platform/graphics/egl/GLContextWrapper.cpp
    platform/graphics/egl/GLDisplay.cpp
    platform/symbian/PlatformDisplaySymbian.cpp
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    "${THIRDPARTY_DIR}/ANGLE/include")
list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/egl")
list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/symbian/PlatformDisplaySymbian.h
)
list(APPEND WebCore_LIBRARIES
    SymbianEGLBridge SymbianNative::EGL SymbianNative::GLES2)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesSymbian.txt"
)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/crypto/mbedtls"
    "${WEBCORE_DIR}/platform/adwaita"
    "${WEBCORE_DIR}/platform/generic"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/symbian"
    "${WEBCORE_DIR}/platform/video-codecs"
)

# The curl backend's certificate handling is written against OpenSSL; the
# Symbian port uses curl's mbedTLS backend instead.
list(REMOVE_ITEM WebCore_SOURCES
    platform/network/curl/CurlSSLVerifier.cpp
    platform/network/curl/OpenSSLHelper.cpp
)
list(APPEND WebCore_SOURCES
    inspector/LegacyWebSocketInspectorInstrumentation.cpp
    platform/network/ResourceHandle.cpp
    platform/network/SynchronousLoaderClient.cpp
    platform/network/curl/ResourceHandleCurl.cpp
    platform/network/curl/CurlSSLVerifierMbedTLS.cpp
    platform/network/curl/MbedTLSHelper.cpp
)
list(REMOVE_ITEM WebCore_LIBRARIES
    OpenSSL::SSL
)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    ${MBEDTLS_INCLUDE_DIR}
)

list(APPEND WebCore_LIBRARIES
    ${MBEDTLS_LIBRARY}
    ${MBEDX509_LIBRARY}
    ${MBEDCRYPTO_LIBRARY}
)
