# WebGL uses ANGLE's validation and GLES renderer over the firmware's EGL/GLES2.
# Native libraries are loaded by ordinal; they must not resolve to ANGLE itself.
list(APPEND ANGLE_DEFINITIONS
    ANGLE_PLATFORM_SYMBIAN
    EGL_NO_PLATFORM_SPECIFIC_TYPES
    USE_SYSTEM_EGL
)
list(APPEND ANGLE_SOURCES
    src/common/system_utils_symbian.cpp
)

if (ENABLE_WEBGL)
    list(REMOVE_ITEM gl_backend_sources
        src/libANGLE/renderer/gl/egl/FunctionsEGLDL.cpp
        src/libANGLE/renderer/gl/egl/FunctionsEGLDL.h
    )
    list(APPEND ANGLE_SOURCES
        ${gl_backend_sources}
        ${libangle_gpu_info_util_sources}
        src/gpu_info_util/SystemInfo_symbian.cpp
        src/common/linux/dma_buf_utils.cpp
        src/libANGLE/renderer/gl/egl/FunctionsEGLSymbian.cpp
    )
    list(APPEND ANGLE_DEFINITIONS ANGLE_ENABLE_OPENGL)

    # WebCore calls GL_* / EGL_* entry points. Do not export public gl* aliases:
    # the native compositor imports those names from the firmware's GLES2 DLL.
    set(libglesv2_sources "")
endif ()

if (USE_ANGLE_EGL)
    message(FATAL_ERROR "Symbian uses private ANGLE EGL_* entry points; public EGL belongs to the firmware")
endif ()
