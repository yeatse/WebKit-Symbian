include(platform/Cairo.cmake)
include(platform/Curl.cmake)
include(platform/FreeType.cmake)
include(platform/ImageDecoders.cmake)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesSymbian.txt"
)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/adwaita"
    "${WEBCORE_DIR}/platform/generic"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/symbian"
)

# The curl backend's certificate handling is written against OpenSSL; the
# Symbian port uses curl's mbedTLS backend instead.
list(REMOVE_ITEM WebCore_SOURCES
    platform/network/curl/CurlSSLVerifier.cpp
    platform/network/curl/OpenSSLHelper.cpp
)
list(APPEND WebCore_SOURCES
    platform/network/curl/CurlSSLVerifierMbedTLS.cpp
    platform/network/curl/MbedTLSHelper.cpp
)
list(REMOVE_ITEM WebCore_LIBRARIES
    OpenSSL::SSL
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/themeAdwaita.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsAdwaita.js
)
set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/rendering/RenderThemeAdwaita.cpp)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    ${MBEDTLS_INCLUDE_DIR}
)

list(APPEND WebCore_LIBRARIES
    ${MBEDTLS_LIBRARY}
    ${MBEDX509_LIBRARY}
    ${MBEDCRYPTO_LIBRARY}
)
