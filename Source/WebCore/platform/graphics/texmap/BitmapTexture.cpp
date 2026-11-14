/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 Copyright (C) 2012, 2025 Igalia S.L.
 Copyright (C) 2012 Adobe Systems Incorporated

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "BitmapTexture.h"

#if USE(TEXTURE_MAPPER)

#include "GLContext.h"
#include "GLFence.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "PlatformDisplay.h"
#include "ProcessCapabilities.h"
#include "TextureMapper.h"
#include "TextureMapperFlags.h"
#include "TextureMapperShaderProgram.h"
#include <cstdio>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if USE(CAIRO)
#include "CairoUtilities.h"
#include "RefPtrCairo.h"
#include <cairo.h>
#include <wtf/text/CString.h>
#endif

#if USE(SKIA)
#include "FontRenderOptions.h"
#include "GraphicsContextSkia.h"
#include "SkiaUtilities.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN // GLib/Win port
#include <skia/core/SkImage.h>
#include <skia/core/SkPixmap.h>
#include <skia/core/SkPicture.h>
#include <skia/core/SkSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

#if OS(DARWIN)
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
static const GLenum s_pixelDataType = GL_UNSIGNED_INT_8_8_8_8_REV;
#else
static const GLenum s_pixelDataType = GL_UNSIGNED_BYTE;
#endif

// On GLES3, the format we want for packed depth stencil is GL_DEPTH24_STENCIL8, but when added through
// the extension this format is called GL_DEPTH24_STENCIL8_OES. In any case they hold the same value 0x88F0
// so we can just use the first one.
// These definitions may not exist if this is a GLES1/2 context without the GL_OES_packed_depth_stencil
// extension. We need to define the one we want to use in order to build on every case.
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

namespace WebCore {

#if OS(SYMBIAN) && USE(SKIA)
OptionSet<BitmapTexture::Flags> BitmapTexture::flagsForContext(OptionSet<Flags> flags)
{
    if (!ProcessCapabilities::canUseAcceleratedBuffers() || flags.contains(Flags::ExternalOESRenderTarget) || flags.contains(Flags::UseBGRALayout))
        return flags;
    auto* display = PlatformDisplay::sharedDisplayIfExists();
    if (!display || !GLContext::current())
        return flags;
    if (!GLContext::isExtensionSupported(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)), "GL_EXT_texture_format_BGRA8888"))
        return flags;
    if (display->skiaGLContext() && display->skiaGrContext()->colorTypeSupportedAsSurface(kBGRA_8888_SkColorType))
        flags.add(Flags::UseBGRALayout);
    return flags;
}
#endif

void BitmapTexture::determineRenderTargetAndBinding()
{
    if (m_flags.contains(Flags::ExternalOESRenderTarget)) {
        m_renderTarget = GL_TEXTURE_EXTERNAL_OES;
        m_binding = GL_TEXTURE_BINDING_EXTERNAL_OES;
        return;
    }

    m_binding = GL_TEXTURE_BINDING_2D;
    m_renderTarget = GL_TEXTURE_2D;
}

GLenum BitmapTexture::textureFormat() const
{
    return m_flags.contains(Flags::UseBGRALayout) ? GL_BGRA : GL_RGBA;
}

GLenum depthBufferFormat()
{
    auto* glContext = GLContext::current();
    if (glContext->version() >= 300 || glContext->glExtensions().OES_packed_depth_stencil)
        return GL_DEPTH24_STENCIL8;

    return GL_DEPTH_COMPONENT16;
}

BitmapTexture::BitmapTexture(const IntSize& size, OptionSet<Flags> flags)
    : m_flags(flags)
    , m_size(size)
    , m_pixelFormat(flags.contains(Flags::UseBGRALayout) ? PixelFormat::BGRA8 : PixelFormat::RGBA8)
{
    determineRenderTargetAndBinding();

#if OS(SYMBIAN)
    RELEASE_ASSERT(GLContext::current());
    m_resourceState = GLContext::current()->resourceState();
#endif
    GLint boundTexture = 0;
    glGetIntegerv(m_binding, &boundTexture);

#if USE(GBM)
    if (m_flags.contains(Flags::BackedByDMABuf)) {
        OptionSet<MemoryMappedGPUBuffer::BufferFlag> bufferFlags;
        if (flags.contains(Flags::ForceLinearBuffer)) {
            ASSERT(!flags.contains(Flags::ForceVivanteSuperTiledBuffer));
            bufferFlags.add(MemoryMappedGPUBuffer::BufferFlag::ForceLinear);
        }

        if (flags.contains(Flags::ForceVivanteSuperTiledBuffer)) {
            ASSERT(!flags.contains(Flags::ForceLinearBuffer));
            bufferFlags.add(MemoryMappedGPUBuffer::BufferFlag::ForceVivanteSuperTiled);
        }

        if (flags.contains(Flags::UseBGRALayout))
            bufferFlags.add(MemoryMappedGPUBuffer::BufferFlag::UseBGRALayout);

        m_memoryMappedGPUBuffer = MemoryMappedGPUBuffer::create(m_size, bufferFlags);

        // Proceed as usual with GL texture creation if the dma-buf creation failed.
        // as we only want to allocate the dma-buf, but neither map it, nor create a texture now - but when we
        // need it (from the thread that needs it!).
        if (allocateTextureFromMemoryMappedGPUBuffer()) {
            glBindTexture(m_renderTarget, boundTexture);
            return;
        }

        m_flags.remove(Flags::BackedByDMABuf);
        m_flags.remove(Flags::ForceLinearBuffer);
        m_flags.remove(Flags::ForceVivanteSuperTiledBuffer);
    }
#endif

    allocateTexture();

    glBindTexture(m_renderTarget, boundTexture);
}

void BitmapTexture::createTexture()
{
    ASSERT(!m_id);
    glGenTextures(1, &m_id);
    glBindTexture(m_renderTarget, m_id);

    GLenum filter = m_flags.contains(Flags::NearestFiltering) ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(m_renderTarget, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(m_renderTarget, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(m_renderTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(m_renderTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void BitmapTexture::allocateTexture()
{
    createTexture();
    // EXT_texture_format_BGRA8888 mandates internalFormat == format.
    // https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_format_BGRA8888.txt
    glTexImage2D(m_renderTarget, 0, textureFormat(), m_size.width(), m_size.height(), 0, textureFormat(), s_pixelDataType, nullptr);

#if OS(SYMBIAN)
    TextureMapper::checkGLError("allocating a texture", m_size);
#endif
}

size_t BitmapTexture::sizeInBytes() const
{
    auto size = allocatedSize();
    if (size.isEmpty())
        return 0;

    const auto bytesPerRow = CheckedUint32(size.width()) * 4;
    return CheckedUint32(size.height()) * bytesPerRow;
}

#if USE(GBM)
IntSize BitmapTexture::allocatedSize() const
{
    return m_memoryMappedGPUBuffer ? m_memoryMappedGPUBuffer->allocatedSize() : m_size;
}

bool BitmapTexture::allocateTextureFromMemoryMappedGPUBuffer()
{
    if (!m_memoryMappedGPUBuffer)
        return false;

    if (auto eglImage = m_memoryMappedGPUBuffer->createEGLImageFromDMABuf()) {
        createTexture();
        glEGLImageTargetTexture2DOES(m_renderTarget, eglImage);

        auto& display = WebCore::PlatformDisplay::sharedDisplay();
        display.destroyEGLImage(eglImage);
        return true;
    }

    LOG_ERROR("Cannot create EGLImage from dma-buf -- rendering will be broken.");
    return false;
}
#endif

#if USE(GBM) || OS(ANDROID)
BitmapTexture::BitmapTexture(EGLImage image, const IntSize& size, OptionSet<Flags> flags)
    : m_flags(flags)
    , m_size(size)
{
    determineRenderTargetAndBinding();

    GLint boundTexture = 0;
    glGetIntegerv(m_binding, &boundTexture);

    createTexture();
    glEGLImageTargetTexture2DOES(m_renderTarget, image);

    glBindTexture(m_renderTarget, boundTexture);
}
#endif

void BitmapTexture::swapTexture(BitmapTexture& other)
{
#if OS(SYMBIAN) && USE(SKIA)
    std::swap(m_hasContents, other.m_hasContents);
#endif
    RELEASE_ASSERT(m_size == other.m_size);
#if OS(SYMBIAN)
    RELEASE_ASSERT(m_resourceState == other.m_resourceState);
#endif
    RELEASE_ASSERT(!m_flags.contains(Flags::DepthBuffer));
    RELEASE_ASSERT(!other.m_flags.contains(Flags::DepthBuffer));

#if USE(GBM)
    std::swap(m_memoryMappedGPUBuffer, other.m_memoryMappedGPUBuffer);
#endif
    std::swap(m_flags, other.m_flags);
    std::swap(m_id, other.m_id);

    determineRenderTargetAndBinding();
    other.determineRenderTargetAndBinding();

    // Take the pixel format from the source texture. The source texture
    // (going back to the pool) is reset to the default pixel format.
    m_pixelFormat = other.m_pixelFormat;
    other.m_pixelFormat = PixelFormat::RGBA8;
}

void BitmapTexture::reset(const IntSize& size, OptionSet<Flags> flags)
{
#if OS(SYMBIAN) && USE(SKIA)
    flags = flagsForContext(flags);
    m_hasContents = false;
#endif
    bool layoutChanged = m_flags.contains(Flags::UseBGRALayout) != flags.contains(Flags::UseBGRALayout);
#if USE(GBM)
    // We don't support switching from dmabuf backing to regular textures -- there is no use-case for that scenario.
    RELEASE_ASSERT(m_flags.contains(Flags::BackedByDMABuf) == flags.contains(Flags::BackedByDMABuf));
#endif

    m_flags = flags;
    m_shouldClear = true;
    m_pixelFormat = flags.contains(Flags::UseBGRALayout) ? PixelFormat::BGRA8 : PixelFormat::RGBA8;
    m_filterOperation = nullptr;

#if OS(SYMBIAN)
    // A framebuffer with only this texture attached serves the next user as
    // it is. Making one is a round trip to the graphics driver, and a pooled
    // surface is reset on every frame it is used.
    bool keepsFramebuffer = !m_depthBufferObject && !m_stencilBufferObject && m_size == size;
#else
    bool keepsFramebuffer = false;
#endif
    if (!flags.contains(Flags::DepthBuffer)) {
        if (m_fbo && !keepsFramebuffer) {
            glDeleteFramebuffers(1, &m_fbo);
            m_fbo = 0;
        }

        if (m_depthBufferObject) {
            glDeleteRenderbuffers(1, &m_depthBufferObject);
            m_depthBufferObject = 0;
        }

        if (m_stencilBufferObject) {
            glDeleteRenderbuffers(1, &m_stencilBufferObject);
            m_stencilBufferObject = 0;
        }

        m_stencilBound = false;
        m_clipStack = { };
    }

    if (m_size == size && !layoutChanged)
        return;
    m_size = size;

    determineRenderTargetAndBinding();

    GLint boundTexture = 0;
    glGetIntegerv(m_binding, &boundTexture);

#if USE(GBM)
    if (m_memoryMappedGPUBuffer) {
        if (m_id) {
            // Recreate GL texture, if it was present before.
            glDeleteTextures(1, &m_id);
            m_id = 0;
        }

        // Recreate MemoryMappedGPUBuffer with new size.
        m_memoryMappedGPUBuffer = MemoryMappedGPUBuffer::create(m_size, m_memoryMappedGPUBuffer->flags());

        if (allocateTextureFromMemoryMappedGPUBuffer()) {
            glBindTexture(m_renderTarget, boundTexture);
            return;
        }
    }
#endif

    glBindTexture(m_renderTarget, m_id);
    glTexImage2D(m_renderTarget, 0, textureFormat(), m_size.width(), m_size.height(), 0, textureFormat(), s_pixelDataType, nullptr);
    glBindTexture(m_renderTarget, boundTexture);

#if OS(SYMBIAN)
    TextureMapper::checkGLError("allocating a texture", m_size);
#endif
}

void BitmapTexture::updateContents(const void* srcData, const IntRect& targetRect, const IntPoint& sourceOffset, int bytesPerLine, PixelFormat pixelFormat)
{
    bool swapRedAndBlue = false;
#if OS(SYMBIAN) && USE(SKIA)
    if (pixelFormat == PixelFormat::BGRA8 && !m_flags.contains(Flags::UseBGRALayout)) {
        swapRedAndBlue = true;
        pixelFormat = PixelFormat::RGBA8;
    }
#endif
    if (m_pixelFormat != pixelFormat) {
        // Only allow pixel format changes, if the whole texture content changes.
        ASSERT(targetRect.size() == m_size);
        ASSERT(targetRect.location().isZero());
        m_pixelFormat = pixelFormat;
    }

#if USE(GBM)
    // Use OpenGL to update multi-plane textures via glTexSubImage2D -- mmap() mode is only intended for single-plane images.
    if (m_memoryMappedGPUBuffer && (m_memoryMappedGPUBuffer->isLinear() || m_memoryMappedGPUBuffer->isVivanteSuperTiled())) {
        RELEASE_ASSERT(sourceOffset.isZero());
        if (auto writeScope = makeGPUBufferWriteScope(*m_memoryMappedGPUBuffer)) {
            m_memoryMappedGPUBuffer->updateContents(*writeScope, srcData, targetRect, bytesPerLine);
            return;
        }

        WTFLogAlways("ERROR: Update Bitmap Texture Contents failed to obtain MemoryMappedGPUBuffer write scope. Aborting fallback to OpenGL..."); // NOLINT
        CRASH();
    }
#endif

    const unsigned bytesPerPixel = 4;
    auto data = static_cast<const uint8_t*>(srcData);
    Vector<uint8_t> temporaryData;
    IntPoint adjustedSourceOffset = sourceOffset;

    // Texture upload requires subimage buffer if driver doesn't support subimage and we don't have full image upload.
    bool supportsUnpackSubimage = GLContext::current()->glExtensions().EXT_unpack_subimage;
#if OS(SYMBIAN)
    // Whole rows starting lower in the source upload in place.
    if (!supportsUnpackSubimage && !adjustedSourceOffset.x() && bytesPerLine == static_cast<int>(targetRect.width() * bytesPerPixel)) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        data += adjustedSourceOffset.y() * bytesPerLine;
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        adjustedSourceOffset = IntPoint();
    }
#endif
    bool requireSubImageBuffer = swapRedAndBlue || (!supportsUnpackSubimage
        && !(bytesPerLine == static_cast<int>(targetRect.width() * bytesPerPixel) && adjustedSourceOffset == IntPoint::zero()));

    // prepare temporaryData if necessary
    if (requireSubImageBuffer) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib/Win port
        size_t subImageSize = targetRect.width() * targetRect.height() * bytesPerPixel;
        uint8_t* dst;
#if OS(SYMBIAN)
        // Every byte is copied over below: reuse one buffer for tile-sized
        // uploads rather than allocate and clear one per tile per frame.
        static constexpr size_t reusedSubImageSize = 256 * 256 * bytesPerPixel;
        static NeverDestroyed<std::unique_ptr<uint8_t[]>> subImageBuffer;
        if (subImageSize <= reusedSubImageSize) {
            if (!subImageBuffer.get())
                subImageBuffer.get() = std::unique_ptr<uint8_t[]>(new uint8_t[reusedSubImageSize]);
            dst = subImageBuffer.get().get();
        } else
#endif
        {
            temporaryData.resize(subImageSize);
            dst = temporaryData.mutableSpan().data();
        }
        data = dst;
        auto bits = static_cast<const uint8_t*>(srcData);
        auto src = bits + sourceOffset.y() * bytesPerLine + sourceOffset.x() * bytesPerPixel;
        const int targetBytesPerLine = targetRect.width() * bytesPerPixel;
        for (int y = 0; y < targetRect.height(); ++y) {
            if (swapRedAndBlue) {
                for (int x = 0; x < targetBytesPerLine; x += bytesPerPixel) {
                    dst[x] = src[x + 2];
                    dst[x + 1] = src[x + 1];
                    dst[x + 2] = src[x];
                    dst[x + 3] = src[x + 3];
                }
            } else
                memcpy(dst, src, targetBytesPerLine);
            src += bytesPerLine;
            dst += targetBytesPerLine;
        }
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

        bytesPerLine = targetBytesPerLine;
        adjustedSourceOffset = IntPoint(0, 0);
    }

    GLint boundTexture = 0;
    glGetIntegerv(m_binding, &boundTexture);

    glBindTexture(m_renderTarget, m_id);

    if (supportsUnpackSubimage) {
        // Use the OpenGL sub-image extension, now that we know it's available.
        glPixelStorei(GL_UNPACK_ROW_LENGTH, bytesPerLine / bytesPerPixel);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, adjustedSourceOffset.y());
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, adjustedSourceOffset.x());
    }

    glTexSubImage2D(m_renderTarget, 0, targetRect.x(), targetRect.y(), targetRect.width(), targetRect.height(), textureFormat(), s_pixelDataType, data);

#if OS(SYMBIAN)
    if (TextureMapper::checkingGLErrors()) {
        char step[64];
        snprintf(step, sizeof(step), "uploading %dx%d at %d,%d into a texture", targetRect.width(), targetRect.height(), targetRect.x(), targetRect.y());
        TextureMapper::checkGLError(step, m_size);
    }
#endif

    if (supportsUnpackSubimage) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    }

    glBindTexture(m_renderTarget, boundTexture);
#if OS(SYMBIAN) && USE(SKIA)
    m_hasContents = true;
#endif
}

#if OS(SYMBIAN) && USE(SKIA)
static bool s_skiaRasterizationEnabled = true;

void BitmapTexture::setSkiaRasterizationEnabled(bool enabled)
{
    s_skiaRasterizationEnabled = enabled;
}

bool BitmapTexture::canRasterizeWithSkia(const IntRect& targetRect) const
{
    if (!s_skiaRasterizationEnabled || !ProcessCapabilities::canUseAcceleratedBuffers() || m_flags.contains(Flags::ExternalOESRenderTarget))
        return false;
    auto* display = PlatformDisplay::sharedDisplayIfExists();
    if (!display || !display->skiaGLContext())
        return false;
    auto layout = m_flags.contains(Flags::UseBGRALayout) ? PixelFormat::BGRA8 : PixelFormat::RGBA8;
    return !m_hasContents || m_pixelFormat == layout || targetRect == IntRect(IntPoint(), m_size);
}

bool BitmapTexture::paintWithSkia(const IntRect& targetRect, NOESCAPE const Function<void(GraphicsContext&)>& paint)
{
    if (!canRasterizeWithSkia(targetRect))
        return false;

    auto& display = PlatformDisplay::sharedDisplay();
    auto* skiaContext = display.skiaGLContext();
    auto* grContext = display.skiaGrContext();
    std::unique_ptr<GLFence> fence;
    bool painted = false;
    // In the context that owns this texture, command order replaces the waits.
    const bool sameContext = skiaContext->resourceState() == m_resourceState;
    const bool hadContents = m_hasContents;
    return m_resourceState->paintAndUpload([&] {
        std::unique_ptr<GLFence> readFence;
        if (!sameContext) {
            // Shared-context writes must wait for the compositor's preceding reads.
            auto& glDisplay = display.glDisplay();
            readFence = GLFence::isSupported(glDisplay) ? GLFence::create(glDisplay) : nullptr;
            if (readFence)
                glFlush();
            else
                glFinish();
        }
        GLContext::ScopedGLContextCurrent current(*skiaContext);
        if (!skiaContext->makeContextCurrent())
            return;
        if (readFence)
            readFence->serverWait();
        grContext->resetContext();
        auto surface = createSkiaSurface(grContext, kTopLeft_GrSurfaceOrigin);
        if (!surface)
            return;
        auto* canvas = surface->getCanvas();
        if (!m_hasContents)
            canvas->clear(SK_ColorTRANSPARENT);
        {
            SkAutoCanvasRestore restore(canvas, true);
            GraphicsContextSkia context(*canvas, RenderingMode::Accelerated, RenderingPurpose::DOM);
            context.clip(targetRect);
            context.clearRect(targetRect);
            paint(context);
        }
        if (!skiaContext->makeContextCurrent())
            return;
        if (sameContext)
            grContext->flushAndSubmit(surface.get(), GrSyncCpu::kNo);
        else
            fence = SkiaUtilities::flushAndSubmitSurfaceWithFence(grContext, surface.get());
        painted = true;
    }, [&] {
        if (!painted)
            return false;
        if (fence)
            fence->serverWait();

        // Sampling parameters stay with the texture: a repaint in the same
        // context has nothing to set again.
        if (!sameContext || !hadContents) {
            GLint boundTexture = 0;
            glGetIntegerv(m_binding, &boundTexture);
            glBindTexture(m_renderTarget, m_id);
            auto filter = m_flags.contains(Flags::NearestFiltering) ? GL_NEAREST : GL_LINEAR;
            glTexParameteri(m_renderTarget, GL_TEXTURE_MIN_FILTER, filter);
            glTexParameteri(m_renderTarget, GL_TEXTURE_MAG_FILTER, filter);
            glTexParameteri(m_renderTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(m_renderTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(m_renderTarget, boundTexture);
        }

        m_pixelFormat = m_flags.contains(Flags::UseBGRALayout) ? PixelFormat::BGRA8 : PixelFormat::RGBA8;
        m_hasContents = true;
        m_shouldClear = false;
        m_clipStack.reset(IntRect(IntPoint(), m_size), ClipStack::YAxisMode::Default);
        return true;
    });
}

bool BitmapTexture::updateContents(SkPicture& picture, const IntRect& targetRect, const IntPoint& offset)
{
    return paintWithSkia(targetRect, [&](GraphicsContext& context) {
        context.translate(targetRect.x() - offset.x(), targetRect.y() - offset.y());
        context.platformContext()->drawPicture(&picture);
    });
}
#endif

void BitmapTexture::updateContents(NativeImage* frameImage, const IntRect& targetRect, const IntPoint& offset)
{
    if (!frameImage)
        return;

#if USE(CAIRO)
    cairo_surface_t* surface = frameImage->platformImage().get();
    const uint8_t* imageData = cairo_image_surface_get_data(surface);
    int bytesPerLine = cairo_image_surface_get_stride(surface);

    updateContents(imageData, targetRect, offset, bytesPerLine, PixelFormat::BGRA8);
#elif USE(SKIA)
    sk_sp<SkImage> surface = frameImage->platformImage();
    SkPixmap pixmap;
    if (surface->peekPixels(&pixmap))
        updateContents(pixmap.addr(), targetRect, offset, pixmap.rowBytes(), PixelFormat::BGRA8);
#if OS(SYMBIAN)
    else if (surface->isTextureBacked()) {
        if (paintWithSkia(targetRect, [&](GraphicsContext& context) {
            context.drawNativeImage(*frameImage, targetRect, FloatRect(offset, targetRect.size()), { });
        }))
            return;

        auto* display = PlatformDisplay::sharedDisplayIfExists();
        auto* skiaContext = display ? display->skiaGLContext() : nullptr;
        if (!skiaContext)
            return;
        auto* grContext = display->skiaGrContext();
        Vector<uint8_t> pixels(targetRect.area().value() * 4);
        auto info = SkImageInfo::Make(targetRect.width(), targetRect.height(), kBGRA_8888_SkColorType, kPremul_SkAlphaType);
        bool copied = false;
        m_resourceState->paintAndUpload([&] {
            GLContext::ScopedGLContextCurrent current(*skiaContext);
            if (!skiaContext->makeContextCurrent())
                return;
            auto image = surface->isValid(grContext->asRecorder()) ? surface : SkiaUtilities::rewrapImageForContext(grContext, *surface);
            copied = image && image->readPixels(grContext, info, pixels.mutableSpan().data(), targetRect.width() * 4, offset.x(), offset.y());
        }, [&] {
            if (copied)
                updateContents(pixels.span().data(), targetRect, IntPoint(), targetRect.width() * 4, PixelFormat::BGRA8);
            return copied;
        });
    }
#endif
#else
    UNUSED_PARAM(targetRect);
    UNUSED_PARAM(offset);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

bool BitmapTexture::updateContents(GraphicsLayer* sourceLayer, const IntRect& targetRect, const IntPoint& offset, float scale)
{
#if OS(SYMBIAN) && USE(SKIA)
    if (paintWithSkia(targetRect, [&](GraphicsContext& context) {
        context.setTextDrawingMode(TextDrawingMode::Fill);
        IntRect sourceRect(offset, targetRect.size());
        sourceRect.scale(1 / scale);
        context.applyDeviceScaleFactor(scale);
        context.translate(targetRect.x() / scale - sourceRect.x(), targetRect.y() / scale - sourceRect.y());
        sourceLayer->paintGraphicsLayerContents(context, sourceRect);
    })) {
        static bool logged;
        if (!logged) {
            WTFLogAlways("Symbian Skia: Ganesh GLES2 layer rasterization");
            logged = true;
        }
        return true;
    }
#endif
    // Making an unconditionally unaccelerated buffer here is OK because this code
    // isn't used by any platforms that respect the accelerated bit.
    auto imageBuffer = ImageBuffer::create(targetRect.size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer)
        return false;

    GraphicsContext& context = imageBuffer->context();
    context.setTextDrawingMode(TextDrawingMode::Fill);

    IntRect sourceRect(targetRect);
    sourceRect.setLocation(offset);
    sourceRect.scale(1 / scale);
    context.applyDeviceScaleFactor(scale);
    context.translate(-sourceRect.x(), -sourceRect.y());

    auto paint = [&] {
        sourceLayer->paintGraphicsLayerContents(context, sourceRect);
    };
    auto upload = [&] {
        auto image = ImageBuffer::sinkIntoNativeImage(WTF::move(imageBuffer));
        if (!image)
            return false;
        updateContents(image.get(), targetRect, IntPoint());
        return true;
    };
#if OS(SYMBIAN)
    return m_resourceState->paintAndUpload(paint, upload);
#else
    paint();
    return upload();
#endif
}

void BitmapTexture::initializeStencil()
{
    if (m_flags.contains(Flags::DepthBuffer)) {
        // We have a depth buffer and we're asked to have a stencil buffer as well. This is only
        // possible if packed depth stencil is available. If that's the case, just bind the depth
        // buffer as the stencil one if haven't done so. If packed depth stencil is not available
        // don't do anything, which will cause stencil clips on this surface to fail.
        if (depthBufferFormat() == GL_DEPTH24_STENCIL8 && !m_stencilBound) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthBufferObject);
            m_stencilBound = true;
        }
        return;
    }

    // We don't have a depth buffer. Use a stencil only buffer.
    if (m_stencilBufferObject)
        return;

    glGenRenderbuffers(1, &m_stencilBufferObject);
    glBindRenderbuffer(GL_RENDERBUFFER, m_stencilBufferObject);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, m_size.width(), m_size.height());
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_stencilBufferObject);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
#if OS(SYMBIAN)
    TextureMapper::checkGLError("adding a stencil buffer to a surface", m_size);
#endif
}

void BitmapTexture::initializeDepthBuffer()
{
    if (m_depthBufferObject)
        return;

    glGenRenderbuffers(1, &m_depthBufferObject);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthBufferObject);
    glRenderbufferStorage(GL_RENDERBUFFER, depthBufferFormat(), m_size.width(), m_size.height());
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBufferObject);
#if OS(SYMBIAN)
    TextureMapper::checkGLError("adding a depth buffer to a surface", m_size);
#endif
}

void BitmapTexture::clearIfNeeded()
{
    if (!m_shouldClear)
        return;

    m_clipStack.reset(IntRect(IntPoint::zero(), m_size), ClipStack::YAxisMode::Default);
    m_clipStack.applyIfNeeded();
    glClearColor(0, 0, 0, 0);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    m_shouldClear = false;
}

void BitmapTexture::createFboIfNeeded()
{
    if (m_fbo)
        return;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_renderTarget, id(), 0);
    if (m_flags.contains(Flags::DepthBuffer))
        initializeDepthBuffer();
    m_shouldClear = true;
}

void BitmapTexture::bindAsSurface()
{
    glBindTexture(m_renderTarget, 0);
    createFboIfNeeded();
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_size.width(), m_size.height());
    if (m_flags.contains(Flags::DepthBuffer))
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    clearIfNeeded();
#if OS(SYMBIAN) && USE(SKIA)
    m_hasContents = true;
#endif
    m_clipStack.apply();
}

BitmapTexture::~BitmapTexture()
{
#if OS(SYMBIAN)
    m_resourceState->release(SymbianGLResourceState::Type::Texture, m_id);
    m_resourceState->release(SymbianGLResourceState::Type::Framebuffer, m_fbo);
    m_resourceState->release(SymbianGLResourceState::Type::Renderbuffer, m_depthBufferObject);
    m_resourceState->release(SymbianGLResourceState::Type::Renderbuffer, m_stencilBufferObject);
#else
    glDeleteTextures(1, &m_id);

    if (m_fbo)
        glDeleteFramebuffers(1, &m_fbo);

    if (m_depthBufferObject)
        glDeleteRenderbuffers(1, &m_depthBufferObject);

    if (m_stencilBufferObject)
        glDeleteRenderbuffers(1, &m_stencilBufferObject);
#endif
}

void BitmapTexture::copyFromExternalTexture(GLuint sourceTextureID, const IntRect& targetRect, const IntSize& sourceOffset)
{
    RELEASE_ASSERT(sourceOffset.width() + targetRect.width() <= m_size.width());
    RELEASE_ASSERT(sourceOffset.height() + targetRect.height() <= m_size.height());

    auto pixelFormat = m_flags.contains(Flags::UseBGRALayout) ? PixelFormat::BGRA8 : PixelFormat::RGBA8;
    if (m_pixelFormat != pixelFormat) {
        // Only allow pixel format changes, if the whole texture content changes.
        ASSERT(targetRect.size() == m_size);
        ASSERT(targetRect.location().isZero());
        m_pixelFormat = pixelFormat;
    }

    GLint boundFramebuffer = 0;
    GLint boundActiveTexture = 0;
    GLint boundTextureOnOriginalUnit = 0;

    determineRenderTargetAndBinding();

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFramebuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &boundActiveTexture);
    glGetIntegerv(m_binding, &boundTextureOnOriginalUnit);

    glBindTexture(m_renderTarget, sourceTextureID);

    GLuint copyFbo = 0;
    glGenFramebuffers(1, &copyFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, copyFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_renderTarget, sourceTextureID, 0);

    glActiveTexture(GL_TEXTURE0);

    // Save GL_TEXTURE0's binding separately when switching away from a different unit.
    GLint boundTextureOnUnit0 = 0;
    if (static_cast<GLenum>(boundActiveTexture) != GL_TEXTURE0)
        glGetIntegerv(m_binding, &boundTextureOnUnit0);

    glBindTexture(m_renderTarget, id());
    glCopyTexSubImage2D(m_renderTarget, 0, targetRect.x(), targetRect.y(), sourceOffset.width(), sourceOffset.height(), targetRect.width(), targetRect.height());

    if (static_cast<GLenum>(boundActiveTexture) != GL_TEXTURE0)
        glBindTexture(m_renderTarget, boundTextureOnUnit0);

    glBindFramebuffer(GL_FRAMEBUFFER, boundFramebuffer);
    glActiveTexture(boundActiveTexture);
    glBindTexture(m_renderTarget, boundTextureOnOriginalUnit);
    glDeleteFramebuffers(1, &copyFbo);
#if OS(SYMBIAN) && USE(SKIA)
    m_hasContents = true;
#endif
}

OptionSet<TextureMapperFlags> BitmapTexture::colorConvertFlags() const
{
    if (m_pixelFormat == PixelFormat::RGBA8)
        return { };

    if (m_flags.contains(Flags::UseBGRALayout))
        return { };

    // Our GL textures are stored in RGBA format. If we received an update in BGRA format, we write that BGRA data into
    // the RGBA GL texture without pixel format conversions, but instead use a shader program to transparently handle
    // the color conversion on-the-fly, when painting the texture.
#if CPU(LITTLE_ENDIAN)
    return TextureMapperFlags::ShouldConvertTextureBGRAToRGBA;
#else
    return TextureMapperFlags::ShouldConvertTextureARGBToRGBA;
#endif
}

#if USE(SKIA)
GrBackendTexture BitmapTexture::createSkiaBackendTexture() const
{
    return SkiaUtilities::createBackendTexture(*this);
}

sk_sp<SkSurface> BitmapTexture::createSkiaSurface(GrDirectContext* grContext, GrSurfaceOrigin origin, unsigned sampleCount) const
{
    auto properties = FontRenderOptions::singleton().createSurfaceProps();
    return SkiaUtilities::createSurface(grContext, *this, properties, origin, sampleCount);
}
#endif

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER)
