/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/Timer.h>
#include <memory>
#include <wtf/RefPtr.h>

namespace WebCore {
class GLContext;
class GraphicsLayer;
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
    std::unique_ptr<WebCore::GLContext> m_context;
    std::unique_ptr<WebCore::TextureMapper> m_textureMapper;
    RefPtr<WebCore::GraphicsLayer> m_root;
    RefPtr<WebCore::GraphicsLayer> m_content;
    RefPtr<WebCore::GraphicsLayer> m_overlay;
    WebCore::Timer m_flushTimer;
    bool m_rendering { false };
    bool m_logNextPresentation { true };
};
}
#endif
