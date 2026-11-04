/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "SymbianCompositor.h"

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
#include "WebViewImpl.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <WebCore/DocumentView.h>
#include <WebCore/BitmapTexturePool.h>
#include <WebCore/GLContext.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayerTextureMapper.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformDisplaySymbian.h>
#include <WebCore/TextureMapper.h>
#include <WebCore/TextureMapperLayer.h>
#include <wtf/SetForScope.h>

namespace WebKitSymbian {
using namespace WebCore;

std::unique_ptr<SymbianCompositor> SymbianCompositor::create(WebViewImpl& view, void* window)
{
    auto compositor = std::unique_ptr<SymbianCompositor>(new SymbianCompositor(view, window));
    if (!compositor->initialize())
        return nullptr;
    return compositor;
}

SymbianCompositor::SymbianCompositor(WebViewImpl& view, void* window)
    : m_view(view)
    , m_nativeWindow(window)
    , m_flushTimer([this] { m_view.requestCompositedDisplay(); })
    , m_stats("egl", { "update", "backing", "draw", "swap" }, 4)
{
}

static void logExtensions(const char* name, const char* extensions)
{
    // Log long lists in lines of whole names; the browser log truncates long lines.
    constexpr size_t lineLength = 200;
    const char* next = extensions ? extensions : "";
    for (unsigned line = 0; *next; ++line) {
        while (*next == ' ')
            ++next;
        const char* start = next;
        const char* end = next;
        while (*next && (next == start || static_cast<size_t>(next - start) < lineLength)) {
            const char* word = next;
            while (*next && *next != ' ')
                ++next;
            if (word != start && static_cast<size_t>(next - start) > lineLength) {
                next = word;
                break;
            }
            end = next;
            while (*next == ' ')
                ++next;
        }
        if (end == start)
            break;
        WTFLogAlways("Symbian compositor: %s[%u]: %.*s", name, line, static_cast<int>(end - start), start);
    }
}

bool SymbianCompositor::initialize()
{
    if (!m_nativeWindow)
        return false;
    if (!PlatformDisplay::sharedDisplayIfExists()) {
        auto display = PlatformDisplaySymbian::create();
        if (!display)
            return false;
        PlatformDisplay::setSharedDisplay(WTF::move(display));
    }
    auto& display = PlatformDisplay::sharedDisplay();
    m_context = GLContext::create(display.glDisplay(), GLContext::Target::Default, nullptr, m_nativeWindow);
    if (!m_context || !m_context->makeContextCurrent()) {
        WTFLogAlways("Symbian compositor: EGL window context failed: %s", GLContext::lastErrorString());
        return false;
    }
    m_textureMapper = TextureMapper::create();
    m_root = GraphicsLayer::create(nullptr, *this);
    m_root->setDrawsContent(true);
    m_root->setContentsOpaque(true);
    m_root->setSize(m_view.size());
    m_root->setNeedsDisplay();
    WTFLogAlways("Symbian compositor: native EGL/GLES2 initialized, renderer=%s", glGetString(GL_RENDERER));
    static bool loggedExtensions;
    if (!loggedExtensions) {
        // Partial update, preserved swaps and EGLImage decide later work.
        loggedExtensions = true;
        EGLDisplay eglDisplay = eglGetCurrentDisplay();
        WTFLogAlways("Symbian compositor: EGL %s, GL %s", eglQueryString(eglDisplay, EGL_VERSION), glGetString(GL_VERSION));
        logExtensions("EGL_EXTENSIONS", eglQueryString(eglDisplay, EGL_EXTENSIONS));
        logExtensions("GL_EXTENSIONS", reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
        EGLint swapBehavior = 0;
        eglQuerySurface(eglDisplay, eglGetCurrentSurface(EGL_DRAW), EGL_SWAP_BEHAVIOR, &swapBehavior);
        WTFLogAlways("Symbian compositor: swap behavior %s", swapBehavior == EGL_BUFFER_PRESERVED ? "preserved" : "destroyed");
    }
    return true;
}

SymbianCompositor::~SymbianCompositor()
{
    prepareForDestruction();
    // GPU-backed layers must release their textures before their GL context.
    if (m_root)
        m_root->removeAllChildren();
    m_content = nullptr;
    m_overlay = nullptr;
    m_root = nullptr;
    m_textureMapper = nullptr;
    if (m_context)
        BitmapTexturePool::singleton().releaseTexturesForContext(m_context->resourceState());
}

void SymbianCompositor::prepareForDestruction()
{
    m_flushTimer.stop();
    if (m_context && !m_context->makeContextCurrent())
        m_context->abandonResources();
}

void SymbianCompositor::updateChildren()
{
    m_root->removeAllChildren();
    if (m_content)
        m_root->addChild(*m_content);
    // View overlays are above document content.
    if (m_overlay)
        m_root->addChild(*m_overlay);
    scheduleRenderingUpdate();
}

void SymbianCompositor::setRootLayer(GraphicsLayer* layer)
{
    if (m_content == layer)
        return;
    m_content = layer;
    updateChildren();
}

void SymbianCompositor::setOverlayLayer(GraphicsLayer* layer)
{
    if (m_overlay == layer)
        return;
    m_overlay = layer;
    updateChildren();
}

void SymbianCompositor::invalidate(const IntRect& rect)
{
    m_root->setNeedsDisplayInRect(rect);
    scheduleRenderingUpdate();
}

void SymbianCompositor::scheduleRenderingUpdate()
{
    if (!m_rendering && !m_flushTimer.isActive())
        m_flushTimer.startOneShot(0_s);
}

void SymbianCompositor::notifyFlushRequired(const GraphicsLayer*) { scheduleRenderingUpdate(); }
void SymbianCompositor::notifySubsequentFlushRequired(const GraphicsLayer*) { scheduleRenderingUpdate(); }

bool SymbianCompositor::resize()
{
    if (m_root->size() == FloatSize(m_view.size()))
        return true;
    if (!m_context->resizeWindowSurface(m_nativeWindow)) {
        WTFLogAlways("Symbian compositor: EGL resize failed: %s", GLContext::lastErrorString());
        return false;
    }
    m_root->setSize(m_view.size());
    m_root->setNeedsDisplay();
    scheduleRenderingUpdate();
    m_logNextPresentation = true;
    return true;
}

bool SymbianCompositor::render()
{
    if (m_rendering || m_view.size().isEmpty())
        return true;
    SetForScope rendering(m_rendering, true);
    m_flushTimer.stop();
    const bool timing = frameStatsEnabled();
    MonotonicTime start = timing ? MonotonicTime::now() : MonotonicTime();
    m_view.page().isolatedUpdateRendering();
    if (!m_context->makeContextCurrent()) {
        WTFLogAlways("Symbian compositor: EGL make-current failed: %s", GLContext::lastErrorString());
        return false;
    }
    auto* frameView = m_view.page().localMainFrame()->view();
    if (!frameView)
        return true;
    m_root->flushCompositingStateForThisLayerOnly();
    if (!frameView->flushCompositingStateIncludingSubframes()) {
        m_flushTimer.startOneShot(16_ms);
        return true;
    }
    if (m_overlay)
        m_overlay->flushCompositingState(FloatRect(FloatPoint(), m_root->size()));
    // Flushing a canvas can use ANGLE. Restore native GL before any backing
    // texture allocation/upload, not just before the final draw.
    if (!m_context->makeContextCurrent())
        return false;
    MonotonicTime updated = timing ? MonotonicTime::now() : MonotonicTime();
    auto& root = downcast<GraphicsLayerTextureMapper>(*m_root);
    // Backing includes Cairo painting of dirty layer tiles and their upload.
    if (!root.updateBackingStoreIncludingSubLayers(*m_textureMapper)) {
        WTFLogAlways("Symbian compositor: backing upload failed, using software painting");
        return false;
    }
    // A canvas preparation can use an ANGLE context during layer flushing.
    if (!m_context->makeContextCurrent())
        return false;
    MonotonicTime backed = timing ? MonotonicTime::now() : MonotonicTime();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_view.size().width(), m_view.size().height());
    // Root backing includes ordinary noncomposited content; clearing also
    // prevents stale edge pixels after a resize or transparent page paint.
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    m_textureMapper->beginPainting();
    root.layer().paint(*m_textureMapper);
    m_textureMapper->endPainting();
    auto error = glGetError();
    if (error != GL_NO_ERROR) {
        WTFLogAlways("Symbian compositor: GLES draw failed (0x%x), using software painting", error);
        return false;
    }
    MonotonicTime drawn = timing ? MonotonicTime::now() : MonotonicTime();
    if (!m_context->swapBuffersChecked()) {
        WTFLogAlways("Symbian compositor: EGL swap failed: %s", GLContext::lastErrorString());
        return false;
    }
    if (timing) {
        MonotonicTime swapped = MonotonicTime::now();
        double since = m_lastFrame ? (swapped - m_lastFrame).milliseconds() : 0;
        m_lastFrame = swapped;
        const double durations[FrameStats::maximumStages] { (updated - start).milliseconds(),
            (backed - updated).milliseconds(), (drawn - backed).milliseconds(), (swapped - drawn).milliseconds() };
        if (m_stats.add(durations, since)) {
            char summary[256];
            m_stats.format(summary, sizeof(summary));
            WTFLogAlways("%s", summary);
        }
    }
    if (m_logNextPresentation) {
        WTFLogAlways("Symbian compositor: native EGL/GLES2 presented %dx%d", m_view.size().width(), m_view.size().height());
        m_logNextPresentation = false;
    }
    if (root.layer().descendantsOrSelfHaveRunningAnimations())
        m_flushTimer.startOneShot(16_ms);
    return true;
}

void SymbianCompositor::paintContents(const GraphicsLayer&, GraphicsContext& context, const FloatRect& rect, OptionSet<GraphicsLayerPaintBehavior>)
{
    context.save();
    context.clip(rect);
    if (auto* view = m_view.page().localMainFrame()->view())
        view->paint(context, enclosingIntRect(rect));
    else
        context.fillRect(rect, Color::white);
    context.restore();
    // BitmapTexture restores and checks its owning context before uploading.
    // Its failure result propagates through the backing store to render().
}

float SymbianCompositor::deviceScaleFactor() const { return m_view.page().deviceScaleFactor(); }
}
#endif
