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

#pragma once

#if OS(SYMBIAN) && ENABLE(WEBGL)

#include "GLContextWrapper.h"
#include "GraphicsContextGLANGLE.h"

namespace WebCore {

class SymbianWebGLLayer;

class GraphicsContextGLSymbianANGLE final : public GraphicsContextGLANGLE, public GLContextWrapper {
    WTF_MAKE_TZONE_ALLOCATED(GraphicsContextGLSymbianANGLE);
public:
    static RefPtr<GraphicsContextGLSymbianANGLE> create(GraphicsContextGLAttributes);
    ~GraphicsContextGLSymbianANGLE();

    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final;
    void prepareForDisplay() final;
    RefPtr<PixelBuffer> readCompositedResults() final;
    bool reshapeDrawingBuffer() final;

    GLContextWrapper::Type type() const final { return GLContextWrapper::Type::Angle; }
    bool makeCurrentImpl() final;
    bool unmakeCurrentImpl() final;
    unsigned glVersion() const final { return 200; }

#if ENABLE(VIDEO)
    bool copyTextureFromVideoFrame(VideoFrame&, PlatformGLObject, GCGLenum, GCGLint, GCGLenum, GCGLenum, GCGLenum, bool, bool) final;
#endif
#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
    RefPtr<VideoFrame> surfaceBufferToVideoFrame(SurfaceBuffer) final;
#endif

private:
    explicit GraphicsContextGLSymbianANGLE(GraphicsContextGLAttributes);
    bool platformInitializeContext() final;
    bool platformInitialize() final;

    std::unique_ptr<SymbianWebGLLayer> m_layer;
    RefPtr<GraphicsLayerContentsDisplayDelegate> m_delegate;
    RefPtr<PixelBuffer> m_displayBuffer;
};

} // namespace WebCore

#endif
