/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
#include <WebCore/GraphicsLayerClient.h>
#include "WebKitSymbianFrameStats.h"
#include <WebCore/Timer.h>
#include <cstdint>
#include <memory>
#include <wtf/MonotonicTime.h>
#include <wtf/RefPtr.h>
#include <wtf/symbian/MainThreadProfile.h>

namespace WebCore {
class Damage;
class GLContext;
class GraphicsLayer;
class SkiaCompositingLayer;
class TextureMapper;
}

namespace WebKitSymbian {
class WebViewImpl;

class SymbianCompositor final : public WebCore::GraphicsLayerClient {
public:
    static std::unique_ptr<SymbianCompositor> create(WebViewImpl&, void* nativeWindow);
    ~SymbianCompositor();
    void setRootLayer(WebCore::GraphicsLayer*);
    void setOverlayLayer(WebCore::GraphicsLayer*);
    void invalidate(const WebCore::IntRect&);
    void scheduleRenderingUpdate();
    bool resize();
    bool render();
    void prepareForDestruction();
    // Off keeps a context of the compositor's own beside Skia's.
    static void setSharedGLContextEnabled(bool);

private:
    explicit SymbianCompositor(WebViewImpl&, void*);
    bool initialize();
    void updateChildren();
    void notifyFlushRequired(const WebCore::GraphicsLayer*) final;
    void notifySubsequentFlushRequired(const WebCore::GraphicsLayer*) final;
    void paintContents(const WebCore::GraphicsLayer&, WebCore::GraphicsContext&, const WebCore::FloatRect&, OptionSet<WebCore::GraphicsLayerPaintBehavior>) final;
    float deviceScaleFactor() const final;

    WebViewImpl& m_view;
    void* m_nativeWindow;
    // Skia's own context drawing into the window, or a context of the
    // compositor's that shares textures with it.
    WebCore::GLContext* m_context { nullptr };
    std::unique_ptr<WebCore::GLContext> m_ownedContext;
    std::unique_ptr<WebCore::TextureMapper> m_textureMapper;
    RefPtr<WebCore::GraphicsLayer> m_root;
    RefPtr<WebCore::GraphicsLayer> m_content;
    RefPtr<WebCore::GraphicsLayer> m_overlay;
    WebCore::Timer m_flushTimer;
    WebCore::Timer m_glCheckTimer;
    bool m_rendering { false };
    bool m_logNextPresentation { true };
    bool glStageFailed(const char* stage);
    bool retryAfterGLError();
    void checkGLAfterFrames();
    void setCheckGLStages(bool);
    bool initializeSkiaPresentation();
    bool composeSkiaFrame(WebCore::SkiaCompositingLayer&, bool& hasRunningAnimations, bool& emptyDamage);
    bool composeSkiaFrameIntoWindow(WebCore::SkiaCompositingLayer&, bool& hasRunningAnimations, bool& emptyDamage);
    bool presentSkiaFrame();
    bool wrapWindowForSkia();
    void recordDamage(const WebCore::Damage&, bool fullDamage, bool emptyDamage);
    void releaseSkiaFrame();

    struct SkiaFrame;
    std::unique_ptr<SkiaFrame> m_skiaFrame;
    bool m_forceFullDamage { true };
    bool m_needsPresentation { true };
    bool m_logSkiaInitialization { true };
    unsigned m_frameNumber { 0 };
    unsigned m_lastDamageKind { 3 };
    unsigned m_fullDamageFrames { 0 };
    unsigned m_partialDamageFrames { 0 };
    unsigned m_emptyDamageFrames { 0 };
    uint64_t m_damagedPixels { 0 };

    // Set after a GL error: each stage and each GL step of drawing then
    // checks for errors, to name the one that fails if it happens again.
    bool m_checkGLStages { false };
    unsigned m_cleanCheckedFrames { 0 };
    unsigned m_framesSinceGLCheck { 0 };
    unsigned m_glFailures { 0 };
    FrameStats m_stats;
    MonotonicTime m_lastFrame;

    // The render stage's main-thread time by kind, summed since the last log.
    void logRenderSplit();
    double m_renderSplit[WTF::MainThreadProfile::categoryCount] { };
    double m_worstRenderSplit[WTF::MainThreadProfile::categoryCount] { };
    double m_worstRender { 0 };
    unsigned m_renderSplitFrames { 0 };
};
}
#endif
