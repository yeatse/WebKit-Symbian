# Native driver imports for the WebKit ARMv6 softfp build. Both soft and
# softfp use the base AAPCS for arguments; hard-float is not compatible with
# the Symbian DLLs. SDK graphics targets currently only accept soft code.
# Keep native EGL headers confined to SymbianEGLBridge: WebCore and ANGLE
# use Khronos pointer handles, while the SDK declares 32-bit integer handles.
include_guard(GLOBAL)

foreach(api IN ITEMS EGL GLES2)
    if(api STREQUAL "EGL")
        set(dll libegl)
    else()
        set(dll libglesv2)
    endif()
    set(proxy "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
    if(NOT EXISTS "${proxy}")
        message(FATAL_ERROR "Symbian ${api} driver import is missing: ${proxy}")
    endif()
    add_library(SymbianNative${api} INTERFACE)
    add_library(SymbianNative::${api} ALIAS SymbianNative${api})
    target_link_libraries(SymbianNative${api} INTERFACE "${proxy}")
endforeach()
