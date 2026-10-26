# The Symbian port's embedding layer: a single-process WebView on WebCore.
list(REMOVE_ITEM WebKitLegacy_SOURCES WebCoreSupport/WebViewGroup.cpp)
# The common list spells this file as only a case-insensitive file system finds it.
list(REMOVE_ITEM WebKitLegacy_SOURCES Storage/InprocessIDBServer.cpp)
list(APPEND WebKitLegacy_SOURCES Storage/InProcessIDBServer.cpp)

list(APPEND WebKitLegacy_SOURCES
    symbian/BackForwardList.cpp
    symbian/SymbianChromeClient.cpp
    symbian/SymbianEditorClient.cpp
    symbian/SymbianFrameLoaderClient.cpp
    symbian/SymbianPlatformStrategies.cpp
    symbian/WebViewImpl.cpp
)

list(APPEND WebKitLegacy_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKITLEGACY_DIR}/symbian"
)

list(APPEND WebKitLegacy_PUBLIC_FRAMEWORK_HEADERS
    symbian/WebKitSymbian.h
)

# The shared WebKitLegacy sources expect a prefix header instead of config.h.
set_property(SOURCE ${WebKitLegacy_SOURCES} APPEND PROPERTY COMPILE_OPTIONS
    -include "${WEBKITLEGACY_DIR}/symbian/WebKitPrefix.h")
