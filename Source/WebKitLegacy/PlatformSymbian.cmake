# The Symbian port's embedding layer: a single-process WebView on WebCore.
list(REMOVE_ITEM WebKitLegacy_SOURCES WebCoreSupport/WebViewGroup.cpp)

list(APPEND WebKitLegacy_SOURCES
    WebCoreSupport/LegacySocketProvider.cpp
    WebCoreSupport/SocketStreamHandle.cpp
    WebCoreSupport/WebSocketChannel.cpp
    symbian/SocketStreamHandleImplCurl.cpp
    symbian/WebResourceLoadSchedulerSymbian.cpp
    symbian/BackForwardList.cpp
    symbian/SymbianChromeClient.cpp
    symbian/SymbianCompositor.cpp
    symbian/SymbianEditorClient.cpp
    symbian/SymbianFrameLoaderClient.cpp
    symbian/SymbianPlatformStrategies.cpp
    symbian/WebViewImpl.cpp
)

list(APPEND WebKitLegacy_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKITLEGACY_DIR}/symbian"
    "${THIRDPARTY_DIR}/ANGLE/include"
)

list(APPEND WebKitLegacy_PUBLIC_FRAMEWORK_HEADERS
    symbian/WebKitSymbian.h
)

if (ENABLE_VIDEO)
    list(APPEND WebKitLegacy_SOURCES symbian/MediaPlayerPrivateSymbian.cpp)
endif ()

# The shared WebKitLegacy sources expect a prefix header instead of config.h.
set_property(SOURCE ${WebKitLegacy_SOURCES} APPEND PROPERTY COMPILE_OPTIONS
    -include "${WEBKITLEGACY_DIR}/symbian/WebKitPrefix.h")
