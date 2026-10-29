if (ENABLE_WEBGL)
    # This implementation presents a CPU snapshot to the native TextureMapper;
    # it does not require sharing native GL objects with ANGLE.
    list(REMOVE_ITEM WebCore_SOURCES
        platform/graphics/texmap/GraphicsContextGLTextureMapperANGLE.cpp
        platform/graphics/texmap/TextureMapperGCGLPlatformLayer.cpp
    )
    list(APPEND WebCore_SOURCES
        platform/graphics/angle/GraphicsContextGLSymbianANGLE.cpp
        platform/graphics/cairo/GraphicsContextGLCairo.cpp
    )
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/angle/GraphicsContextGLSymbianANGLE.h
    )
endif ()
