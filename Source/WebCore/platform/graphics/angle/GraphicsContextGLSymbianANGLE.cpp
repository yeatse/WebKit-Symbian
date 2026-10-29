/*
 * Copyright (C) 2026 The WebKit-Symbian Authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsContextGLSymbianANGLE.h"

#if OS(SYMBIAN) && ENABLE(WEBGL)

#include "ANGLEHeaders.h"
#include "BitmapTexture.h"
#include "NativeImage.h"
#include "PixelBuffer.h"
#include "PlatformLayerDisplayDelegate.h"
#include "TextureMapper.h"
#include "TextureMapperFlags.h"
#include "TextureMapperPlatformLayer.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GraphicsContextGLSymbianANGLE);

// Keep presentation independent of native EGL context sharing. A WebGL frame
// is read once, then uploaded while the compositor's own context is current.
class SymbianWebGLLayer final : public TextureMapperPlatformLayer {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(SymbianWebGLLayer);
public:
    ~SymbianWebGLLayer()
    {
        if (client())
            client()->platformLayerWillBeDestroyed();
    }

    void setImage(RefPtr<NativeImage>&& image)
    {
        m_image = WTF::move(image);
        if (client())
            client()->setPlatformLayerNeedsDisplay();
    }

    void paintToTextureMapper(TextureMapper& mapper, const FloatRect& target, const TransformationMatrix& transform, float opacity) final
    {
        if (!m_image)
            return;
        // The temporary texture is created and released on the native context.
        // It must not be retained and later deleted while ANGLE is current.
        auto size = m_image->size();
        Ref texture = BitmapTexture::create(size, { BitmapTexture::Flags::SupportsAlpha });
        texture->updateContents(m_image.get(), IntRect { { }, size }, { });
        mapper.drawTexture(texture->id(), { TextureMapperFlags::ShouldFlipTexture, TextureMapperFlags::ShouldBlend }, target, transform, opacity);
    }

private:
    RefPtr<NativeImage> m_image;
};

static RefPtr<PixelBuffer> copyPixelBuffer(const PixelBuffer& source)
{
    auto copy = source.createScratchPixelBuffer(source.size());
    if (copy)
        copy->setRange(source.bytes(), 0);
    return copy;
}

RefPtr<GraphicsContextGL> createWebProcessGraphicsContextGL(const GraphicsContextGLAttributes& attributes)
{
    return GraphicsContextGLSymbianANGLE::create(attributes);
}

RefPtr<GraphicsContextGLSymbianANGLE> GraphicsContextGLSymbianANGLE::create(GraphicsContextGLAttributes attributes)
{
    if (attributes.isWebGL2)
        return nullptr;
    auto context = adoptRef(*new GraphicsContextGLSymbianANGLE(attributes));
    if (!context->initialize())
        return nullptr;
    return context;
}

GraphicsContextGLSymbianANGLE::GraphicsContextGLSymbianANGLE(GraphicsContextGLAttributes attributes)
    : GraphicsContextGLANGLE(attributes)
{
}

GraphicsContextGLSymbianANGLE::~GraphicsContextGLSymbianANGLE() = default;

GraphicsContextGLANGLE::~GraphicsContextGLANGLE()
{
    if (!m_displayObj)
        return;
    // Destroying an ANGLE context releases its GL objects even after a reset or
    // partial initialization. Do not invoke the derived wrapper during teardown.
    for (auto* image : m_eglImages.values())
        EGL_DestroyImageKHR(m_displayObj, image);
    for (auto* sync : m_eglSyncs.values())
        EGL_DestroySync(m_displayObj, sync);
    if (m_contextObj) {
        if (EGL_GetCurrentContext() == m_contextObj)
            EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        EGL_DestroyContext(m_displayObj, m_contextObj);
    }
    if (m_surfaceObj)
        EGL_DestroySurface(m_displayObj, m_surfaceObj);
}

bool GraphicsContextGLANGLE::makeContextCurrent()
{
    auto& context = static_cast<GraphicsContextGLSymbianANGLE&>(*this);
    if (context.isCurrent())
        return true;
    if (!context.makeCurrentImpl())
        return false;
    context.didMakeContextCurrent();
    return true;
}

bool GraphicsContextGLSymbianANGLE::makeCurrentImpl()
{
    return m_contextObj && EGL_MakeCurrent(m_displayObj, m_surfaceObj, m_surfaceObj, m_contextObj);
}

bool GraphicsContextGLSymbianANGLE::unmakeCurrentImpl()
{
    return EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool GraphicsContextGLSymbianANGLE::platformInitializeContext()
{
    const EGLint displayAttributes[] = {
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, 2,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, 0,
        EGL_NONE
    };
    m_displayObj = EGL_GetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, displayAttributes);
    if (m_displayObj == EGL_NO_DISPLAY || !EGL_Initialize(m_displayObj, nullptr, nullptr))
        return false;
    if (!EGL_BindAPI(EGL_OPENGL_ES_API))
        return false;
    const EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_NONE
    };
    EGLint configCount = 0;
    if (!EGL_ChooseConfig(m_displayObj, configAttributes, &m_configObj, 1, &configCount) || !configCount)
        return false;
    const EGLint surfaceAttributes[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    m_surfaceObj = EGL_CreatePbufferSurface(m_displayObj, m_configObj, surfaceAttributes);
    if (m_surfaceObj == EGL_NO_SURFACE)
        return false;
    const EGLint contextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE, EGL_FALSE,
        EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE, EGL_TRUE,
        EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE, EGL_TRUE,
        EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE, EGL_FALSE,
        EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM, EGL_FALSE,
        EGL_NONE
    };
    m_contextObj = EGL_CreateContext(m_displayObj, m_configObj, EGL_NO_CONTEXT, contextAttributes);
    return makeContextCurrent();
}

bool GraphicsContextGLSymbianANGLE::platformInitialize()
{
    m_layer = makeUnique<SymbianWebGLLayer>();
    m_delegate = PlatformLayerDisplayDelegate::create(m_layer.get());
    return true;
}

RefPtr<GraphicsLayerContentsDisplayDelegate> GraphicsContextGLSymbianANGLE::layerContentsDisplayDelegate()
{
    return m_delegate;
}

bool GraphicsContextGLSymbianANGLE::reshapeDrawingBuffer()
{
    m_displayBuffer = nullptr;
    m_layer->setImage(nullptr);
    auto [target, binding] = drawingBufferTextureBindingPoint();
    ScopedRestoreTextureBinding restore(binding, target);
    GL_BindTexture(target, m_texture);
    auto size = getInternalFramebufferSize();
    auto format = contextAttributes().alpha ? GL_RGBA : GL_RGB;
    GL_TexImage2D(target, 0, format, size.width(), size.height(), 0, format, GL_UNSIGNED_BYTE, nullptr);
    return GL_GetError() == GL_NO_ERROR;
}

void GraphicsContextGLSymbianANGLE::prepareForDisplay()
{
    if (getInternalFramebufferSize().isEmpty()) {
        m_displayBuffer = nullptr;
        m_layer->setImage(nullptr);
        return;
    }
    m_displayBuffer = readRenderingResultsForPainting();
    if (!m_displayBuffer) {
        forceContextLost();
        return;
    }
    auto pixels = copyPixelBuffer(*m_displayBuffer);
    if (!pixels) {
        forceContextLost();
        return;
    }
    m_layer->setImage(createNativeImageFromPixelBuffer(contextAttributes(), pixels.releaseNonNull()));
}

RefPtr<PixelBuffer> GraphicsContextGLSymbianANGLE::readCompositedResults()
{
    return m_displayBuffer ? copyPixelBuffer(*m_displayBuffer) : nullptr;
}

void GraphicsContextGLANGLE::checkGPUStatus()
{
    if (m_failNextStatusCheck) {
        m_failNextStatusCheck = false;
        forceContextLost();
    }
}

void GraphicsContextGLANGLE::platformReleaseThreadResources()
{
    // The common release path unbinds ANGLE. Invalidate the wrapper cache too,
    // so a later call cannot incorrectly skip EGL_MakeCurrent.
    auto* current = GLContextWrapper::currentContext();
    if (current && current->type() == GLContextWrapper::Type::Angle)
        current->didUnmakeContextCurrent();
}

#if ENABLE(VIDEO)
bool GraphicsContextGLSymbianANGLE::copyTextureFromVideoFrame(VideoFrame&, PlatformGLObject, GCGLenum, GCGLint, GCGLenum, GCGLenum, GCGLenum, bool, bool)
{
    return false; // Request the existing image conversion fallback, not a zero-copy import.
}
#endif

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
RefPtr<VideoFrame> GraphicsContextGLSymbianANGLE::surfaceBufferToVideoFrame(SurfaceBuffer)
{
    return nullptr;
}
#endif

} // namespace WebCore

#endif
