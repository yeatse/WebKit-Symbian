/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "SymbianCompositor.h"

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
#include "WebViewImpl.h"
#if ENABLE(VIDEO)
#include "MediaPlayerPrivateSymbian.h"
#endif
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <cstdio>
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
#include <WebCore/TextureMapperTiledBackingStore.h>
#include <wtf/SetForScope.h>
#include <wtf/symbian/MainThreadProfile.h>

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
    , m_stats("egl", { "render", "flush", "backing", "draw", "swap" }, 5)
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
    // With WebCore's layers attached, the document is in them (the root
    // layer of the frame is always composited then): painting the frame into
    // the root as well would paint nothing, at the cost of a whole-view
    // upload. Without them (view overlays only), the root shows the page.
    m_root->setDrawsContent(!m_content);
    if (!m_content)
        m_root->setNeedsDisplay();
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

// Reads every pending GL error flag. A driver may keep several, and one left
// behind would be blamed on a later step.
static GLenum takeGLErrors(char* codes, size_t size)
{
    GLenum first = GL_NO_ERROR;
    size_t used = 0;
    codes[0] = 0;
    for (int i = 0; i < 8; ++i) {
        GLenum error = glGetError();
        if (error == GL_NO_ERROR)
            break;
        if (first == GL_NO_ERROR)
            first = error;
        if (used + 8 < size)
            used += snprintf(codes + used, size - used, "%s0x%x", used ? " " : "", error);
    }
    return first;
}

void SymbianCompositor::setCheckGLStages(bool check)
{
    m_checkGLStages = check;
    m_cleanCheckedFrames = 0;
    TextureMapper::setCheckGLErrors(check);
}

bool SymbianCompositor::glStageFailed(const char* stage)
{
    char codes[48];
    GLenum error = takeGLErrors(codes, sizeof(codes));
    const char* step = TextureMapper::takeGLErrorStep();
    if (error == GL_NO_ERROR && !step)
        return false;
    if (step)
        WTFLogAlways("Symbian compositor: GL error %s while %s%s%s", step, stage, error != GL_NO_ERROR ? ", then " : "", codes);
    else
        WTFLogAlways("Symbian compositor: GL error %s while %s", codes, stage);
    return true;
}

// A checked frame failed: drop it and draw again, up to a few times, before
// leaving the page to software painting.
bool SymbianCompositor::retryAfterGLError()
{
    constexpr unsigned maximumFailures = 3;
    if (++m_glFailures >= maximumFailures) {
        WTFLogAlways("Symbian compositor: %u frames failed in GL, using software painting", m_glFailures);
        return false;
    }
    m_flushTimer.startOneShot(0_s);
    return true;
}

bool SymbianCompositor::render()
{
    if (m_rendering || m_view.size().isEmpty())
        return true;
    SetForScope rendering(m_rendering, true);
    m_flushTimer.stop();
    const bool timing = frameStatsEnabled();
    // Work in the frame outside every finer scope is the compositor's own.
    WTF::MainThreadProfile::Scope profileScope(WTF::MainThreadProfile::Category::Frame);
    auto beforeRendering = WTF::MainThreadProfile::totals();
    MonotonicTime start = timing ? MonotonicTime::now() : MonotonicTime();
    m_view.page().isolatedUpdateRendering();
    if (timing && profileScope.active()) {
        // Split the render stage by kind, to find what runs in each frame.
        auto afterRendering = WTF::MainThreadProfile::totals();
        double frameRender = 0;
        double split[WTF::MainThreadProfile::categoryCount];
        for (unsigned i = 0; i < WTF::MainThreadProfile::categoryCount; ++i) {
            split[i] = afterRendering.milliseconds[i] - beforeRendering.milliseconds[i];
            m_renderSplit[i] += split[i];
            frameRender += split[i];
        }
        ++m_renderSplitFrames;
        if (frameRender > m_worstRender) {
            m_worstRender = frameRender;
            std::copy(std::begin(split), std::end(split), m_worstRenderSplit);
        }
    }
    // Rendering runs animation frame callbacks (canvas and WebGL drawing),
    // style and layout; flushing prepares layers, canvases included.
    MonotonicTime rendered = timing ? MonotonicTime::now() : MonotonicTime();
    if (!m_context->makeContextCurrent()) {
        WTFLogAlways("Symbian compositor: EGL make-current failed: %s", GLContext::lastErrorString());
        return false;
    }
    if (m_checkGLStages) {
        char codes[48];
        if (takeGLErrors(codes, sizeof(codes)) != GL_NO_ERROR)
            WTFLogAlways("Symbian compositor: GL error %s left from before the frame", codes);
        TextureMapper::takeGLErrorStep();
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
    if (m_checkGLStages && glStageFailed("flushing layers"))
        return retryAfterGLError();
    MonotonicTime updated = timing ? MonotonicTime::now() : MonotonicTime();
    auto& root = downcast<GraphicsLayerTextureMapper>(*m_root);
    // Tiles cover only what is on screen and a margin around it, which needs
    // this frame's layer transforms (scroll positions included).
    root.layer().prepareForPainting(*m_textureMapper);
    bool morePending = false;
    TextureMapperTiledBackingStore::beginCoverageFrame();
    // Backing includes Cairo painting of dirty layer tiles and their upload.
    if (!root.updateBackingStoreIncludingSubLayers(*m_textureMapper, FloatRect(FloatPoint(), m_root->size()), morePending)) {
        WTFLogAlways("Symbian compositor: backing upload failed, using software painting");
        return false;
    }
    // A canvas preparation can use an ANGLE context during layer flushing.
    if (!m_context->makeContextCurrent())
        return false;
    if (m_checkGLStages && glStageFailed("painting or uploading tiles"))
        return retryAfterGLError();
    MonotonicTime backed = timing ? MonotonicTime::now() : MonotonicTime();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_view.size().width(), m_view.size().height());
    // Root backing includes ordinary noncomposited content; clearing also
    // prevents stale edge pixels after a resize or transparent page paint.
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
#if ENABLE(VIDEO)
    MediaPlayerPrivateSymbian::willComposite();
#endif
    m_textureMapper->beginPainting();
    root.layer().paint(*m_textureMapper);
    m_textureMapper->endPainting();
#if ENABLE(VIDEO)
    // Before the swap, so the window lets the video through when the
    // frame with its hole is presented.
    m_view.setVideoUnderlayVisible(MediaPlayerPrivateSymbian::didComposite(IntRect(IntPoint(), m_view.size())));
#endif
    if (!m_checkGLStages) {
        char codes[48];
        if (takeGLErrors(codes, sizeof(codes)) != GL_NO_ERROR) {
            // The error may have come from any stage of the frame. Drop the
            // frame and draw again checking each, so a repeat names it.
            WTFLogAlways("Symbian compositor: GL error %s in a frame, drawing again with checks", codes);
            setCheckGLStages(true);
            m_flushTimer.startOneShot(0_s);
            return true;
        }
    } else {
        if (glStageFailed("drawing layers"))
            return retryAfterGLError();
        // Checks cost a GL query per step: stop after a run of clean frames.
        constexpr unsigned cleanFramesToStopChecking = 60;
        if (++m_cleanCheckedFrames >= cleanFramesToStopChecking) {
            WTFLogAlways("Symbian compositor: %u clean frames, GL checks off", m_cleanCheckedFrames);
            setCheckGLStages(false);
            m_glFailures = 0;
        }
    }
    MonotonicTime drawn = timing ? MonotonicTime::now() : MonotonicTime();
    if (!m_context->swapBuffersChecked()) {
        WTFLogAlways("Symbian compositor: EGL swap failed: %s", GLContext::lastErrorString());
        return false;
    }
    if (timing) {
        ++m_drawStatsFrames;
        MonotonicTime swapped = MonotonicTime::now();
        double since = m_lastFrame ? (swapped - m_lastFrame).milliseconds() : 0;
        m_lastFrame = swapped;
        const double durations[FrameStats::maximumStages] { (rendered - start).milliseconds(),
            (updated - rendered).milliseconds(), (backed - updated).milliseconds(), (drawn - backed).milliseconds(), (swapped - drawn).milliseconds() };
        if (m_stats.add(durations, since)) {
            char summary[256];
            m_stats.format(summary, sizeof(summary));
            WTFLogAlways("%s", summary);
            WTFLogAlways("egl-tiles: %zu KB of %zu KB", TextureMapperTiledBackingStore::coverageBytes() / 1024,
                TextureMapperTiledBackingStore::coverageBudgetBytes / 1024);
            auto& backing = TextureMapperTiledBackingStore::coverageStats();
            double backingFrames = std::max(1u, backing.frames);
            // New tiles came into view; damaged ones were repainted in part.
            WTFLogAlways("egl-backing: per frame new=%.1f/%.0fkpx damaged=%.1f/%.0fkpx visible-ms=%.1f prefetch=%.1f/%.0fkpx/%.1fms skipped=%u worst=%.0fms '%s'",
                backing.newTiles / backingFrames, backing.newPixels / backingFrames / 1000,
                backing.damagedTiles / backingFrames, backing.damagedPixels / backingFrames / 1000, backing.visibleMs / backingFrames,
                backing.prefetchTiles / backingFrames, backing.prefetchPixels / backingFrames / 1000, backing.prefetchMs / backingFrames,
                backing.prefetchSkipped, backing.worstMs, backing.worstLayer.utf8().data());
            backing = { };
            auto& draw = TextureMapper::drawStats();
            double frames = std::max(1u, m_drawStatsFrames);
            WTFLogAlways("egl-draw: per frame quads=%.1f kpx=%.0f surfaces=%.1f rounded-clips=%.1f stencil-clips=%.1f shape-clips=%.1f",
                draw.draws / frames, draw.pixels / frames / 1000, draw.surfaces / frames, draw.roundedClips / frames, draw.stencilClips / frames,
                draw.shapeClips / frames);
            // Surfaces counts binds; these count each layer painted through a
            // surface of its own, by reason.
            WTFLogAlways("egl-surfaces: per frame opacity=%.1f filter-mask=%.1f flatten=%.1f backdrop=%.1f filter-passes=%.1f kpx=%.0f",
                draw.opacitySurfaces / frames, draw.filterMaskSurfaces / frames, draw.flattenSurfaces / frames,
                draw.backdropSurfaces / frames, draw.filterPasses / frames, draw.surfacePixels / frames / 1000);
            draw = { };
            m_drawStatsFrames = 0;
            logRenderSplit();
        }
    }
    if (m_logNextPresentation) {
        WTFLogAlways("Symbian compositor: native EGL/GLES2 presented %dx%d", m_view.size().width(), m_view.size().height());
        m_logNextPresentation = false;
    }
    if (root.layer().descendantsOrSelfHaveRunningAnimations())
        m_flushTimer.startOneShot(16_ms);
    else if (morePending) {
        // Paint the next tiles ahead after pending input and timers.
        m_flushTimer.startOneShot(0_s);
    }
    return true;
}

void SymbianCompositor::logRenderSplit()
{
    if (!m_renderSplitFrames)
        return;
    char average[360];
    char worst[360];
    size_t averageUsed = 0;
    size_t worstUsed = 0;
    for (unsigned i = 0; i < WTF::MainThreadProfile::categoryCount; ++i) {
        int written = snprintf(average + averageUsed, sizeof(average) - averageUsed, "%s%s=%.1f", i ? " " : "", WTF::MainThreadProfile::categoryName(i), m_renderSplit[i] / m_renderSplitFrames);
        if (written > 0)
            averageUsed = std::min(sizeof(average) - 1, averageUsed + written);
        written = snprintf(worst + worstUsed, sizeof(worst) - worstUsed, "%s%s=%.0f", i ? " " : "", WTF::MainThreadProfile::categoryName(i), m_worstRenderSplit[i]);
        if (written > 0)
            worstUsed = std::min(sizeof(worst) - 1, worstUsed + written);
    }
    // The render stage only: rAF callbacks, style, layout and what they call.
    WTFLogAlways("egl-render: per frame %s", average);
    WTFLogAlways("egl-render-worst: %.0fms %s", m_worstRender, worst);
    std::fill(std::begin(m_renderSplit), std::end(m_renderSplit), 0);
    m_renderSplitFrames = 0;
    m_worstRender = 0;
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
