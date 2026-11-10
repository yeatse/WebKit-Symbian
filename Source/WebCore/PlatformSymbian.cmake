include(platform/Adwaita.cmake)
include(platform/Skia.cmake)
include(platform/Curl.cmake)
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
    platform/graphics/egl/GLFence.cpp
    platform/graphics/egl/GLFenceEGL.cpp
    platform/graphics/egl/GLFenceGL.cpp
    platform/symbian/PlatformDisplaySymbian.cpp
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    "${THIRDPARTY_DIR}/ANGLE/include")
list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/texmap/coordinated")
list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/image-decoders/ScalableImageDecoder.h
    platform/image-decoders/ScalableImageDecoderFrame.h
    platform/symbian/PlatformDisplaySymbian.h
    platform/graphics/Damage.h
    platform/graphics/skia/SkiaNativeTile.h
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBuffer.h
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferHolePunch.h
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

# Isolate native descriptors/leaves from WebCore's no-exceptions translation units.
list(APPEND WebCore_SOURCES platform/symbian/NetworkStateNotifierSymbianNative.cpp)
set_source_files_properties(platform/symbian/NetworkStateNotifierSymbianNative.cpp PROPERTIES
    INCLUDE_DIRECTORIES "${SYMBIAN_SDK_PREFIX}/include/native"
    SKIP_PRECOMPILE_HEADERS ON
    COMPILE_OPTIONS "-fexceptions;-fno-rtti"
    COMPILE_DEFINITIONS "__SUPPORT_CPP_EXCEPTIONS__;__LEAVE_EQUALS_THROW__")
list(APPEND WebCore_LIBRARIES "${SYMBIAN_SDK_PREFIX}/proxies/connmon/connmon.dso")

# The phone's ICL JPEG decoder, on its own thread with its own scheduler.
list(APPEND WebCore_SOURCES platform/image-decoders/jpeg/JPEGSystemDecoderSymbianNative.cpp)
set_source_files_properties(platform/image-decoders/jpeg/JPEGSystemDecoderSymbianNative.cpp PROPERTIES
    INCLUDE_DIRECTORIES "${SYMBIAN_SDK_PREFIX}/include/native"
    SKIP_PRECOMPILE_HEADERS ON
    COMPILE_OPTIONS "-fexceptions;-fno-rtti"
    COMPILE_DEFINITIONS "__SUPPORT_CPP_EXCEPTIONS__;__LEAVE_EQUALS_THROW__")
list(APPEND WebCore_LIBRARIES
    "${SYMBIAN_SDK_PREFIX}/proxies/imageconversion/imageconversion.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/fbscli/fbscli.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/efsrv/efsrv.dso")

# A real packed real-FFT backend; the complete audio destination is separate.
if (ENABLE_WEB_AUDIO)
    list(APPEND WebCore_SOURCES platform/audio/symbian/FFTFrameSymbian.cpp)
endif ()

# Native PCM utility objects and its scheduler belong to the audio render thread.
if (ENABLE_WEB_AUDIO)
    list(APPEND WebCore_SOURCES
        platform/audio/symbian/AudioDestinationSymbian.cpp
        platform/audio/symbian/AudioDestinationSymbianNative.cpp
    )
    set_source_files_properties(platform/audio/symbian/AudioDestinationSymbianNative.cpp PROPERTIES
        INCLUDE_DIRECTORIES "${SYMBIAN_SDK_PREFIX}/include/native"
        SKIP_PRECOMPILE_HEADERS ON
        COMPILE_OPTIONS "-fexceptions;-fno-rtti"
        COMPILE_DEFINITIONS "__SUPPORT_CPP_EXCEPTIONS__;__LEAVE_EQUALS_THROW__")
    list(APPEND WebCore_LIBRARIES
        "${SYMBIAN_SDK_PREFIX}/proxies/mediaclientaudiostream/mediaclientaudiostream.dso"
        "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso"
    )
endif ()

# Decode supported in-memory WAVE and embed the existing HRTF resources.
# This does not enable WEB_AUDIO; the destination and policy backend are separate.
if (ENABLE_WEB_AUDIO)
    set(_symbianHRTFHeader "${WebCore_DERIVED_SOURCES_DIR}/AudioBusSymbianHRTFData.h")
    set(_symbianHRTFGenerator "${WEBCORE_DIR}/platform/audio/symbian/generate-symbian-hrtf.py")
    file(GLOB _symbianHRTFResponses "${WEBCORE_DIR}/platform/audio/resources/IRC_Composite_C_R0195_T*_P*.wav")
    add_custom_command(
        OUTPUT "${_symbianHRTFHeader}"
        COMMAND ${PYTHON_EXECUTABLE} "${_symbianHRTFGenerator}"
            "${WEBCORE_DIR}/platform/audio/resources" "${_symbianHRTFHeader}"
        DEPENDS "${_symbianHRTFGenerator}"
            "${WEBCORE_DIR}/platform/audio/resources/Composite.wav" ${_symbianHRTFResponses}
        VERBATIM
    )
    list(APPEND WebCore_SOURCES
        platform/audio/symbian/AudioFileReaderSymbian.cpp
        platform/audio/symbian/AudioBusSymbian.cpp
        "${_symbianHRTFHeader}"
    )
    set_source_files_properties(platform/audio/symbian/AudioBusSymbian.cpp PROPERTIES
        OBJECT_DEPENDS "${_symbianHRTFHeader}")
endif ()
