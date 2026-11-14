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
#include <GLES2/gl2ext.h>
#include <cstdio>
#include <WebCore/DocumentView.h>
#include <WebCore/BitmapTexturePool.h>
#include <WebCore/Damage.h>
#include <WebCore/FontRenderOptions.h>
#include <WebCore/GLContext.h>
#include <WebCore/GLFence.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayerTextureMapper.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformDisplaySymbian.h>
#include <WebCore/SkiaCompositingLayer.h>
#include <WebCore/SkiaUtilities.h>
#include <WebCore/TextureMapper.h>
#include <WebCore/TextureMapperLayer.h>
#include <WebCore/TextureMapperTiledBackingStore.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkRegion.h>
#include <skia/core/SkSurface.h>
#include <skia/gpu/ganesh/GrDirectContext.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLTypes.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/symbian/MainThreadProfile.h>

namespace WebKitSymbian {
using namespace WebCore;

struct SymbianCompositor::SkiaFrame {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(SkiaFrame);
public:
    sk_sp<SkSurface> surface;
    GrBackendTexture texture;
    std::unique_ptr<GLFence> producerFence;
    std::unique_ptr<GLFence> consumerFence;
    IntSize size;
    GLuint program { 0 };
    GLuint vertexBuffer { 0 };
    GLint sampler { -1 };
    GLuint textureID { 0 };
};

static bool s_sharedContextEnabled = true;

static GLuint compilePresentationShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled)
        return shader;
    char log[256] { };
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    WTFLogAlways("Symbian Skia compositor: presentation shader failed: %s", log);
    glDeleteShader(shader);
    return 0;
}

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
    , m_glCheckTimer([this] { checkGLAfterFrames(); })
    , m_stats("egl", { "render", "flush", "backing", "compose", "swap" }, 5)
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
    if (TextureMapper::skiaCompositionEnabled() && s_sharedContextEnabled && !SkiaUtilities::compositorSharesGLContext()) {
        auto* skiaContext = display.skiaGLContext();
        if (skiaContext && skiaContext->attachWindowSurface(m_nativeWindow)) {
            m_context = skiaContext;
            m_skiaFrame = makeUnique<SkiaFrame>();
            if (wrapWindowForSkia()) {
                SkiaUtilities::setCompositorSharesGLContext(true);
                WTFLogAlways("Symbian compositor: Skia draws into the window in one GL context");
            } else {
                WTFLogAlways("Symbian compositor: Skia cannot draw into the window, using a context of its own");
                m_skiaFrame = nullptr;
                m_context = nullptr;
                skiaContext->detachWindowSurface();
            }
        }
    }
    if (!m_context) {
        m_ownedContext = GLContext::create(display, m_nativeWindow);
        m_context = m_ownedContext.get();
    }
    if (!m_context || !m_context->makeContextCurrent()) {
        WTFLogAlways("Symbian compositor: EGL window context failed: %s", GLContext::lastErrorString());
        return false;
    }
    m_textureMapper = TextureMapper::create();
    if (m_ownedContext && TextureMapper::skiaCompositionEnabled()) {
        m_skiaFrame = makeUnique<SkiaFrame>();
        if (!initializeSkiaPresentation())
            return false;
    }
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
    releaseSkiaFrame();
    if (m_context && !m_ownedContext) {
        // Skia keeps its context and what it holds; only the window goes.
        if (auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext())
            grContext->flushAndSubmit(GrSyncCpu::kNo);
        m_context->detachWindowSurface();
        SkiaUtilities::setCompositorSharesGLContext(false);
    }
}

void SymbianCompositor::setSharedGLContextEnabled(bool enabled)
{
    s_sharedContextEnabled = enabled;
}

bool SymbianCompositor::initializeSkiaPresentation()
{
    constexpr auto* vertexSource =
        "attribute vec2 a_position; attribute vec2 a_texCoord; varying vec2 v_texCoord;"
        "void main() { gl_Position=vec4(a_position,0.0,1.0); v_texCoord=a_texCoord; }";
    constexpr auto* fragmentSource =
        "precision mediump float; varying vec2 v_texCoord; uniform sampler2D u_texture;"
        "void main() { gl_FragColor=texture2D(u_texture,v_texCoord); }";
    GLuint vertex = compilePresentationShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragment = compilePresentationShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertex || !fragment) {
        if (vertex)
            glDeleteShader(vertex);
        if (fragment)
            glDeleteShader(fragment);
        return false;
    }

    auto& frame = *m_skiaFrame;
    frame.program = glCreateProgram();
    glAttachShader(frame.program, vertex);
    glAttachShader(frame.program, fragment);
    glBindAttribLocation(frame.program, 0, "a_position");
    glBindAttribLocation(frame.program, 1, "a_texCoord");
    glLinkProgram(frame.program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(frame.program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[256] { };
        glGetProgramInfoLog(frame.program, sizeof(log), nullptr, log);
        WTFLogAlways("Symbian Skia compositor: presentation program failed: %s", log);
        return false;
    }
    frame.sampler = glGetUniformLocation(frame.program, "u_texture");
    if (frame.sampler < 0)
        return false;

    // Ganesh's top-left surfaces store their top row at texture v=0.
    constexpr GLfloat vertices[] {
        -1, 1, 0, 0,
        -1, -1, 0, 1,
        1, 1, 1, 0,
        1, -1, 1, 1,
    };
    glGenBuffers(1, &frame.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, frame.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return frame.vertexBuffer && glGetError() == GL_NO_ERROR;
}

void SymbianCompositor::releaseSkiaFrame()
{
    if (!m_skiaFrame)
        return;
    auto& frame = *m_skiaFrame;
    if (auto* display = PlatformDisplay::sharedDisplayIfExists()) {
        if (auto* context = display->skiaGLContext()) {
            GLContext::ScopedGLContextCurrent current(*context);
            if (GLContext::current() == context) {
                if (frame.consumerFence)
                    frame.consumerFence->serverWait();
                frame.consumerFence = nullptr;
                frame.producerFence = nullptr;
                frame.texture = { };
                frame.surface = nullptr;
            }
        }
    }
    if (m_context) {
        auto& resources = m_context->resourceState();
        resources->release(SymbianGLResourceState::Type::Program, frame.program);
        resources->release(SymbianGLResourceState::Type::Buffer, frame.vertexBuffer);
    }
    m_skiaFrame = nullptr;
}

void SymbianCompositor::recordDamage(const Damage& frameDamage, bool fullDamage, bool emptyDamage)
{
    auto& frame = *m_skiaFrame;
    uint64_t pixels = 0;
    SkRegion damageRegion;
    for (const auto& rect : frameDamage.rectsForPainting())
        damageRegion.op(SkIRect(rect), SkRegion::kUnion_Op);
    for (SkRegion::Iterator rects(damageRegion); !rects.done(); rects.next())
        pixels += static_cast<uint64_t>(rects.rect().width()) * rects.rect().height();
    const uint64_t totalPixels = static_cast<uint64_t>(frame.size.width()) * frame.size.height();
    fullDamage |= damageRegion.contains(SkIRect::MakeWH(frame.size.width(), frame.size.height()));
    unsigned damageKind = emptyDamage ? 2 : fullDamage ? 0 : 1;
    ++m_frameNumber;
    if (!damageKind)
        ++m_fullDamageFrames;
    else if (damageKind == 1)
        ++m_partialDamageFrames;
    else
        ++m_emptyDamageFrames;
    m_damagedPixels += pixels;
    if (m_frameNumber <= 12 || (frameStatsEnabled() && damageKind != m_lastDamageKind)) {
        WTFLogAlways("Symbian Skia compositor: frame=%u damage=%s rects=%zu pixels=%llu total=%llu", m_frameNumber,
            emptyDamage ? "empty" : fullDamage ? "full" : "partial", frameDamage.rectsForPainting().size(),
            static_cast<unsigned long long>(pixels), static_cast<unsigned long long>(totalPixels));
        m_lastDamageKind = damageKind;
    }
}

bool SymbianCompositor::wrapWindowForSkia()
{
    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    if (!grContext || !m_context->makeContextCurrent())
        return false;
    auto& frame = *m_skiaFrame;
    GrGLFramebufferInfo framebuffer;
    framebuffer.fFBOID = 0;
    // GLES2 without OES_rgb8_rgba8 renders to BGRA8 only, as Skia sees it.
    // The name is bookkeeping: colors reach the window in GL's own order.
    constexpr GLenum bgra8 = 0x93A1;
    bool useBGRA = grContext->colorTypeSupportedAsSurface(kBGRA_8888_SkColorType);
    framebuffer.fFormat = useBGRA ? bgra8 : GL_RGBA8_OES;
    // The view can still be empty when the compositor starts.
    auto size = m_view.size().isEmpty() ? IntSize(1, 1) : m_view.size();
    auto target = GrBackendRenderTargets::MakeGL(size.width(), size.height(), 0, 8, framebuffer);
    auto properties = FontRenderOptions::singleton().createSurfaceProps();
    frame.surface = SkSurfaces::WrapBackendRenderTarget(grContext, target, kBottomLeft_GrSurfaceOrigin, useBGRA ? kBGRA_8888_SkColorType : kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), &properties);
    frame.size = size;
    return !!frame.surface;
}

// One render pass a frame: the layers are drawn into the window's back
// buffer, which holds nothing from the frame before, so all of them are drawn
// whenever anything changed.
bool SymbianCompositor::composeSkiaFrameIntoWindow(SkiaCompositingLayer& scene, bool& hasRunningAnimations, bool& emptyDamage)
{
    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    if (!grContext)
        return false;
    auto& frame = *m_skiaFrame;
    grContext->resetContext();
    if ((!frame.surface || frame.size != m_view.size()) && !wrapWindowForSkia())
        return false;

    // Without a frame to show, an unchanged scene is found and not drawn.
    std::optional<Damage> frameDamage(std::in_place, frame.size, Damage::Mode::Rectangles);
    std::optional<Damage> priorTargetDamage;
    if (!m_forceFullDamage && !m_needsPresentation)
        priorTargetDamage.emplace(frame.size);
    hasRunningAnimations = scene.paint(*frame.surface->getCanvas(), frameDamage, priorTargetDamage, SK_ColorWHITE, false);
    emptyDamage = priorTargetDamage && frameDamage->isEmpty();
    recordDamage(*frameDamage, !emptyDamage, emptyDamage);
    if (emptyDamage)
        return true;
    grContext->flushAndSubmit(frame.surface.get(), GrSyncCpu::kNo);
    m_forceFullDamage = false;
    m_needsPresentation = true;
    if (m_logSkiaInitialization) {
        WTFLogAlways("Symbian Skia compositor: initialized");
        m_logSkiaInitialization = false;
    }
    return true;
}

bool SymbianCompositor::composeSkiaFrame(SkiaCompositingLayer& scene, bool& hasRunningAnimations, bool& emptyDamage)
{
    auto& display = PlatformDisplay::sharedDisplay();
    auto* context = display.skiaGLContext();
    if (!context)
        return false;
    GLContext::ScopedGLContextCurrent current(*context);
    if (GLContext::current() != context)
        return false;
    auto* grContext = display.skiaGrContext();
    if (!grContext)
        return false;
    auto& frame = *m_skiaFrame;
    if (frame.consumerFence)
        frame.consumerFence->serverWait();
    frame.consumerFence = nullptr;
    frame.texture = { };
    frame.textureID = 0;
    grContext->resetContext();

    if (!frame.surface || frame.size != m_view.size()) {
        auto colorType = grContext->colorTypeSupportedAsSurface(kBGRA_8888_SkColorType) ? kBGRA_8888_SkColorType : kRGBA_8888_SkColorType;
        auto info = SkImageInfo::Make(m_view.size().width(), m_view.size().height(), colorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
        auto properties = FontRenderOptions::singleton().createSurfaceProps();
        frame.surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kYes, info, 0, kTopLeft_GrSurfaceOrigin, &properties);
        if (!frame.surface)
            return false;
        frame.size = m_view.size();
        m_forceFullDamage = true;
    }

    bool fullDamage = m_forceFullDamage;
    std::optional<Damage> frameDamage(std::in_place, frame.size, fullDamage ? Damage::Mode::Full : Damage::Mode::Rectangles);
    std::optional<Damage> priorTargetDamage;
    if (!fullDamage)
        priorTargetDamage.emplace(frame.size);
    hasRunningAnimations = scene.paint(*frame.surface->getCanvas(), frameDamage, priorTargetDamage, SK_ColorWHITE);
    emptyDamage = !fullDamage && frameDamage->isEmpty();

    recordDamage(*frameDamage, fullDamage, emptyDamage);

    if (emptyDamage && !m_needsPresentation)
        return true;
    frame.texture = SkSurfaces::GetBackendTexture(frame.surface.get(), SkSurfaces::BackendHandleAccess::kFlushRead);
    if (!frame.texture.isValid())
        return false;
    GrGLTextureInfo texture;
    if (!GrBackendTextures::GetGLTextureInfo(frame.texture, &texture))
        return false;
    frame.textureID = texture.fID;
    frame.producerFence = SkiaUtilities::flushAndSubmitSurfaceWithFence(grContext, frame.surface.get());
    m_forceFullDamage = false;
    m_needsPresentation = true;
    if (m_logSkiaInitialization) {
        WTFLogAlways("Symbian Skia compositor: initialized");
        m_logSkiaInitialization = false;
    }
    return true;
}

bool SymbianCompositor::presentSkiaFrame()
{
    if (!m_context->makeContextCurrent())
        return false;
    auto& frame = *m_skiaFrame;
    if (frame.producerFence)
        frame.producerFence->serverWait();
    frame.producerFence = nullptr;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_view.size().width(), m_view.size().height());
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(frame.program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, frame.textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GrBackendTextures::GLTextureParametersModified(&frame.texture);
    glUniform1i(frame.sampler, 0);
    glBindBuffer(GL_ARRAY_BUFFER, frame.vertexBuffer);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), reinterpret_cast<const void*>(2 * sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    frame.consumerFence = GLFence::create(PlatformDisplay::sharedDisplay().glDisplay());
    glFlush();
    if (!frame.consumerFence)
        glFinish();
    return true;
}

void SymbianCompositor::prepareForDestruction()
{
    m_flushTimer.stop();
    m_glCheckTimer.stop();
    if (m_ownedContext && !m_ownedContext->makeContextCurrent())
        m_ownedContext->abandonResources();
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
    m_forceFullDamage = true;
    m_needsPresentation = true;
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
// Runs once frames stop coming, so the last ones do not stay unchecked.
void SymbianCompositor::checkGLAfterFrames()
{
    if (m_rendering || m_checkGLStages || !m_context || !m_context->makeContextCurrent())
        return;
    m_framesSinceGLCheck = 0;
    char codes[48];
    if (takeGLErrors(codes, sizeof(codes)) == GL_NO_ERROR)
        return;
    WTFLogAlways("Symbian compositor: GL error %s in the last frames, drawing again with checks", codes);
    setCheckGLStages(true);
    m_flushTimer.startOneShot(0_s);
}

bool SymbianCompositor::retryAfterGLError()
{
    m_forceFullDamage = true;
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
    // Backing includes Skia painting of dirty layer tiles and their upload.
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
    bool hasRunningAnimations = false;
    if (TextureMapper::skiaCompositionEnabled()) {
        auto& scene = root.layer().syncSkiaLayer(*m_textureMapper);
#if ENABLE(VIDEO)
        MediaPlayerPrivateSymbian::willComposite();
#endif
        bool emptyDamage = false;
        bool composed = m_ownedContext ? composeSkiaFrame(scene, hasRunningAnimations, emptyDamage)
            : composeSkiaFrameIntoWindow(scene, hasRunningAnimations, emptyDamage);
        if (!composed) {
            WTFLogAlways("Symbian Skia compositor: frame composition failed");
            return false;
        }
#if ENABLE(VIDEO)
        m_view.setVideoUnderlayVisible(MediaPlayerPrivateSymbian::didComposite(IntRect(IntPoint(), m_view.size())));
#endif
        if (emptyDamage && !m_needsPresentation) {
            if (hasRunningAnimations)
                m_flushTimer.startOneShot(16_ms);
            else if (morePending)
                m_flushTimer.startOneShot(0_s);
            return true;
        }
        if (m_ownedContext && !presentSkiaFrame())
            return false;
    } else {
        // Layers are drawn straight into the window: one render pass and no
        // wait for the GPU.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_view.size().width(), m_view.size().height());
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
#if ENABLE(VIDEO)
        MediaPlayerPrivateSymbian::willComposite();
#endif
        m_textureMapper->beginPaintingToWindow(m_view.size());
        root.layer().paint(*m_textureMapper);
        m_textureMapper->endPainting();
#if ENABLE(VIDEO)
        m_view.setVideoUnderlayVisible(MediaPlayerPrivateSymbian::didComposite(IntRect(IntPoint(), m_view.size())));
#endif
        hasRunningAnimations = root.layer().descendantsOrSelfHaveRunningAnimations();
    }
    if (!m_checkGLStages) {
        // Reading the error flag waits for the driver to take in the whole
        // frame. The flag stays set until read, so a failed frame is still
        // found, a few frames late.
        constexpr unsigned framesBetweenGLChecks = 8;
        char codes[48];
        bool checks = ++m_framesSinceGLCheck >= framesBetweenGLChecks;
        if (checks)
            m_framesSinceGLCheck = 0;
        else
            m_glCheckTimer.startOneShot(100_ms);
        if (checks && takeGLErrors(codes, sizeof(codes)) != GL_NO_ERROR) {
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
    m_needsPresentation = false;
    if (timing) {
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
            unsigned compositionFrames = m_fullDamageFrames + m_partialDamageFrames + m_emptyDamageFrames;
            if (compositionFrames)
                WTFLogAlways("skia-composition: frames=%u full=%u partial=%u skipped=%u damage-kpx=%.1f",
                    compositionFrames, m_fullDamageFrames, m_partialDamageFrames, m_emptyDamageFrames,
                    static_cast<double>(m_damagedPixels) / compositionFrames / 1000);
            m_fullDamageFrames = m_partialDamageFrames = m_emptyDamageFrames = 0;
            m_damagedPixels = 0;
            logRenderSplit();
        }
    }
    if (m_logNextPresentation) {
        WTFLogAlways("Symbian compositor: native EGL/GLES2 presented %dx%d", m_view.size().width(), m_view.size().height());
        m_logNextPresentation = false;
    }
    if (hasRunningAnimations)
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
