/*
 * Copyright (C) 2026 The symbian-webkit authors.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebViewImpl.h"

#include "BackForwardList.h"
#include "LegacySocketProvider.h"
#include "NetworkStorageSessionMap.h"
#include "PageStorageSessionProvider.h"
#include "SymbianChromeClient.h"
#include "SymbianCompositor.h"
#include "WebKitSymbianFrameStats.h"
#if ENABLE(VIDEO)
#include "MediaPlayerPrivateSymbian.h"
#endif
#include "SymbianEditorClient.h"
#include "SymbianFrameLoaderClient.h"
#include "SymbianPlatformStrategies.h"
#include "WebBroadcastChannelRegistry.h"
#include "WebDatabaseProvider.h"
#include "WebStorageNamespaceProvider.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/VM.h>
#include <WebCore/BackForwardCache.h>
#include <WebCore/BackForwardController.h>
#include <WebCore/CacheStorageProvider.h>
#include <WebCore/CompositionHighlight.h>
#include <WebCore/CompositionUnderline.h>
#include <WebCore/CommonVM.h>
#include <WebCore/CookieJar.h>
#include <WebCore/CookieJarDB.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/DocumentView.h>
#include <WebCore/NodeDocument.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/DummySpeechRecognitionProvider.h>
#include <WebCore/Editor.h>
#include <WebCore/UserGestureIndicator.h>
#include <WebCore/EmptyClients.h>
#include <WebCore/GraphicsContextCairo.h>
#include <WebCore/ScalableImageDecoder.h>
#include <WebCore/FrameTreeSyncData.h>
#include <WebCore/EventNames.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FocusController.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLFormControlElement.h>
#include <WebCore/HandleUserInputEventResult.h>
#include <WebCore/ScrollingCoordinatorTypes.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/MemoryRelease.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/Page.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PlatformTouchEvent.h>
#include <WebCore/RemoteFrameGeometryTransformer.h>
#include <WebCore/PointerCaptureController.h>
#include <WebCore/PointerEvent.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/RenderObjectInlines.h>
#include <WebCore/RenderStyle+GettersInlines.h>
#include <WebCore/StyleTouchAction.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/ProgressTrackerClient.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/Settings.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/SocketProvider.h>
#include <WebCore/SubstituteData.h>
#include <WebCore/TextIterator.h>
#include <WebCore/TextureMapperLayer.h>
#include <WebCore/UserContentController.h>
#include <WebCore/VisitedLinkStore.h>
#include <WebCore/WindowsKeyboardCodes.h>
#include <cairo.h>
#include <wtf/FileSystem.h>
#include <wtf/HashSet.h>
#include <wtf/HexNumber.h>
#include <wtf/MainThread.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/RunLoop.h>
#include <wtf/symbian/MemoryPressureHandlerSymbian.h>
#include <wtf/symbian/MainThreadProfile.h>
#include <wtf/symbian/SymbianSystem.h>
#include <wtf/text/StringBuilder.h>

namespace WebKitSymbian {

using namespace WebCore;

static String& dataDirectory()
{
    static NeverDestroyed<String> directory;
    return directory;
}

namespace {

String toString(Text text)
{
    if (!text.characters || text.length <= 0)
        return emptyString();
    return String(std::span { reinterpret_cast<const char16_t*>(text.characters), static_cast<size_t>(text.length) });
}

class SymbianProgressTrackerClient final : public ProgressTrackerClient {
public:
    explicit SymbianProgressTrackerClient(WebViewImpl& view)
        : m_view(view)
    {
    }

private:
    void progressStarted(LocalFrame&) final { m_view.progressChanged(); }
    void progressEstimateChanged(LocalFrame&) final { m_view.progressChanged(); }
    void progressFinished(LocalFrame&) final { m_view.progressChanged(); }

    WebViewImpl& m_view;
};

class SymbianVisitedLinkStore final : public VisitedLinkStore {
public:
    static Ref<SymbianVisitedLinkStore> create() { return adoptRef(*new SymbianVisitedLinkStore); }

private:
    bool isLinkVisited(Page&, SharedStringHash hash, const URL&, const AtomString&) final { return m_hashes.contains(hash); }
    void addVisitedLink(Page&, SharedStringHash hash) final
    {
        m_hashes.add(hash);
        invalidateStylesForLink(hash);
    }

    HashSet<SharedStringHash> m_hashes;
};

// The key and legacy keyIdentifier values of a Windows key code.
String keyForKeyCode(int keyCode, const String& text)
{
    switch (keyCode) {
    case VK_BACK: return "Backspace"_s;
    case VK_TAB: return "Tab"_s;
    case VK_RETURN: return "Enter"_s;
    case VK_ESCAPE: return "Escape"_s;
    case VK_PRIOR: return "PageUp"_s;
    case VK_NEXT: return "PageDown"_s;
    case VK_END: return "End"_s;
    case VK_HOME: return "Home"_s;
    case VK_LEFT: return "ArrowLeft"_s;
    case VK_UP: return "ArrowUp"_s;
    case VK_RIGHT: return "ArrowRight"_s;
    case VK_DOWN: return "ArrowDown"_s;
    case VK_DELETE: return "Delete"_s;
    }
    return text.isEmpty() ? "Unidentified"_s : text;
}

String keyIdentifierForKeyCode(int keyCode, const String& text)
{
    switch (keyCode) {
    case VK_RETURN: return "Enter"_s;
    case VK_END: return "End"_s;
    case VK_HOME: return "Home"_s;
    case VK_LEFT: return "Left"_s;
    case VK_UP: return "Up"_s;
    case VK_RIGHT: return "Right"_s;
    case VK_DOWN: return "Down"_s;
    case VK_PRIOR: return "PageUp"_s;
    case VK_NEXT: return "PageDown"_s;
    case VK_DELETE: return "U+007F"_s;
    }
    UChar character = keyCode ? keyCode : (text.isEmpty() ? 0 : text[0]);
    return makeString("U+"_s, hex(toASCIIUpper(character), 4));
}

} // namespace

} // namespace WebKitSymbian

String WebDatabaseProvider::indexedDatabaseDirectoryPath()
{
    return FileSystem::pathByAppendingComponent(WebKitSymbian::dataDirectory(), "IndexedDB"_s);
}

#if ENABLE(WEB_AUDIO)
// From WebCore's private AudioDestinationSymbianNative.h.
namespace WebCore::SymbianAudio {
void setOutputVolume(double);
}
#endif

namespace WebKitSymbian {

UTF16Text::UTF16Text(const String& string)
{
    m_characters.reserveInitialCapacity(string.length());
    for (unsigned i = 0; i < string.length(); ++i)
        m_characters.append(string[i]);
}

Text UTF16Text::text() const
{
    return Text { reinterpret_cast<const uint16_t*>(m_characters.span().data()), static_cast<int>(m_characters.size()) };
}

// Process.

void WebView::initializeProcess(const char* directory)
{
    static bool initialized;
    if (initialized)
        return;
    initialized = true;

    JSC::initialize();
    WTF::initializeMainThread();
    SymbianPlatformStrategies::initialize();

    dataDirectory() = String::fromUTF8(directory);
    FileSystem::makeAllDirectories(dataDirectory());
#if ENABLE(VIDEO)
    MediaPlayerPrivateSymbian::registerMediaEngine(dataDirectory());
#endif
    NetworkStorageSessionMap::defaultStorageSession().setCookieDatabase(
        makeUniqueRef<CookieJarDB>(FileSystem::pathByAppendingComponent(dataDirectory(), "cookies.db"_s)));


    // A phone's worth of caches.
    MemoryCache::singleton().setCapacities(0, 4 * MB, 8 * MB);
    BackForwardCache::singleton().setMaxSize(1);
}

void WebView::setWakeUpCallback(void (*wakeUp)(void*), void* context)
{
    if (!wakeUp) {
        WTF::stopMemoryPressureMonitoring();
        MemoryPressureHandler::singleton().setLowMemoryHandler({ });
        RunLoop::setWakeUpCallback({ });
        return;
    }
    RunLoop::setWakeUpCallback([wakeUp, context] {
        wakeUp(context);
    });
    auto& pressureHandler = MemoryPressureHandler::singleton();
    pressureHandler.setLowMemoryHandler([](Critical critical, Synchronous synchronous) {
        ASSERT(isMainThread());
        // Protect the VM while observing its actual JS heap. These numbers are
        // neither process RSS nor a limit; this callback never forces a test GC.
        RefPtr<JSC::VM> protectedVM = commonVMOrNull();
        auto logHeap = [&](const char* phase) {
            if (!protectedVM)
                return;
            JSC::JSLockHolder lock(protectedVM.get());
            size_t heapBytes = protectedVM->heap.size();
            size_t extraBytes = protectedVM->heap.extraMemorySize();
            size_t cellBytes = heapBytes >= extraBytes ? heapBytes - extraBytes : 0;
            WTFLogAlways("Symbian memory pressure: JS heap phase=%s cellBytes=%zu extraBytes=%zu", phase, cellBytes, extraBytes);
        };
        logHeap("before");
        // Release the diagnostic JS lock before WebCore calls its own GC/cache
        // routines. Their usual asynchronous GC policy preserves all JS roots.
        WebCore::releaseMemory(critical, synchronous, MaintainBackForwardCache::No, MaintainMemoryCache::No);
        logHeap("after-scheduling");
    });
    pressureHandler.install();
}

static bool s_frameStatsEnabled;

bool frameStatsEnabled() { return s_frameStatsEnabled; }

void WebView::setFrameStatsEnabled(bool enabled)
{
    s_frameStatsEnabled = enabled;
    WebCore::ScalableImageDecoder::setDecodeStatsEnabled(enabled);
    WTF::MainThreadProfile::setEnabled(enabled);
}

static bool s_tiledCompositingEnabled = true;

void WebView::setCompositedVideoEnabled(bool enabled)
{
#if ENABLE(VIDEO)
    MediaPlayerPrivateSymbian::setCompositedVideoEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

void WebView::setWebAudioVolume(double volume)
{
#if ENABLE(WEB_AUDIO)
    WebCore::SymbianAudio::setOutputVolume(volume);
#else
    UNUSED_PARAM(volume);
#endif
}

void WebView::setTiledCompositingEnabled(bool enabled)
{
    s_tiledCompositingEnabled = enabled;
}

void WebView::setBackdropFiltersEnabled(bool enabled)
{
    WebCore::TextureMapperLayer::setBackdropFiltersEnabled(enabled);
}

void WebView::setCompilerThreadsShareUIPriority(bool share)
{
    WTF::SymbianSystem::setCompilerThreadsShareUIPriority(share);
}

void WebView::setMediaPlayerFactory(MediaPlayerFactory factory, void* context)
{
#if ENABLE(VIDEO)
    MediaPlayerPrivateSymbian::setFactory(factory, context);
#else
    UNUSED_PARAM(factory);
    UNUSED_PARAM(context);
#endif
}

void WebView::runPendingWork()
{
    RunLoop::cycle();
}

int64_t WebView::microsecondsUntilNextTimer()
{
    MonotonicTime fireTime = RunLoop::nextTimerFireTime();
    if (fireTime == MonotonicTime::infinity())
        return -1;
    Seconds delay = fireTime - MonotonicTime::now();
    return delay > 0_s ? static_cast<int64_t>(delay.microseconds()) : 0;
}

WebView* WebView::create(WebViewClient& client, int width, int height)
{
    return new WebViewImpl(client, width, height);
}

// View.

WebViewImpl::WebViewImpl(WebViewClient& client, int width, int height)
    : m_client(client)
    , m_downloads(SymbianDownloadManager::create(client))
    , m_size(width, height)
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    , m_compositorIdleTimer([this] { requestCompositedDisplay(); })
#endif
    , m_scaleTimer([this] { updateScale(); })
{
    auto storageProvider = PageStorageSessionProvider::create();
    auto configuration = pageConfigurationWithEmptyClients(PageIdentifier::generate(), PAL::SessionID::defaultSessionID());
    configuration.editorClient = makeUniqueRef<SymbianEditorClient>(*this);
    configuration.socketProvider = LegacySocketProvider::create();
    configuration.userContentProvider = UserContentController::create();
    configuration.backForwardClient = BackForwardList::create();
    configuration.cookieJar = CookieJar::create(storageProvider.copyRef());
    configuration.progressTrackerClient = makeUniqueRef<SymbianProgressTrackerClient>(*this);
    configuration.mainFrameCreationParameters = PageConfiguration::LocalMainFrameCreationParameters {
        [this] (auto& frame, auto& loader) -> UniqueRef<LocalFrameLoaderClient> {
            return makeUniqueRefWithoutRefCountedCheck<SymbianFrameLoaderClient>(*this, frame, loader);
        }, { }, ReferrerPolicy::EmptyString
    };
    configuration.chromeClient = makeUniqueRef<SymbianChromeClient>(*this);
    configuration.databaseProvider = &WebDatabaseProvider::singleton();
    configuration.storageNamespaceProvider = WebKit::WebStorageNamespaceProvider::create(
        FileSystem::pathByAppendingComponent(dataDirectory(), "LocalStorage"_s));
    configuration.broadcastChannelRegistry = WebBroadcastChannelRegistry::getOrCreate(configuration.sessionID.isEphemeral());
    configuration.visitedLinkStore = SymbianVisitedLinkStore::create();

    m_page = Page::create(WTF::move(configuration));
    storageProvider->setPage(*m_page);

    Settings& settings = m_page->settings();
    settings.setScriptEnabled(true);
    settings.setDownloadAttributeEnabled(true);
    // Direct Page construction uses WebCore's disabled storage defaults,
    // even though the Legacy storage namespace is supplied above.
    settings.setLocalStorageEnabled(true);
    settings.setSessionStorageEnabled(true);
#if ENABLE(WEBGL)
    // This port creates Page directly; WebCore defaults WebGL to disabled.
    settings.setWebGLEnabled(true);
#endif
#if ENABLE(WEB_AUDIO)
    // The experimental build has the real classic PCM/decoder backend. Keep
    // Worklet interfaces disabled until its asynchronous teardown is complete.
    settings.setWebAudioEnabled(true);
    settings.setAudioWorkletEnabled(false);
#endif
    // HTML leaves "allowed to play" to the user agent. Like other mobile
    // browsers, audible media and AudioContext start only from user
    // activation; muted media may still autoplay.
    settings.setRequiresUserGestureForAudioPlayback(true);
    settings.setServiceWorkersEnabled(false);
    settings.setSharedWorkerEnabled(false);
    settings.setCacheAPIEnabled(false);
    settings.setLoadsImagesAutomatically(true);
    settings.setTextAreasAreResizable(false);
    settings.setDefaultTextEncodingName("UTF-8"_s);
    // Enable only after the embedding has supplied a working native window.
    settings.setAcceleratedCompositingEnabled(false);

    m_page->localMainFrame()->init();
    m_page->setIsVisible(true);
    m_page->focusController().setActive(true);
}

WebViewImpl::~WebViewImpl()
{
    m_client.cancelAuthenticationChallenge(0);
    m_downloads->detach();
    m_scaleTimer.stop();
    cancelTouches();
    disableCompositing();
    m_page->localMainFrame()->loader().detachFromParent();
}

// The embedding presents ordinary pages itself (ScreenPlay or bitmap). EGL
// composition starts only while WebCore attaches composited layers, from the
// embedding's next renderCompositedFrame(), after it has released its own
// window surface. It ends when the layers have been gone for a while.
static constexpr Seconds compositorIdleTimeout = 2_s;

bool WebViewImpl::setNativeWindow(void* window)
{
    disableCompositing();
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    if (window) {
        m_nativeWindow = window;
        // The frame's content then lives in a layer whose tiles follow the
        // scroll position (SymbianCompositor).
        m_page->settings().setForceCompositingMode(s_tiledCompositingEnabled);
        // Fixed and sticky boxes get layers of their own, which move with the
        // scroll instead of being painted again into the document's tiles.
        m_page->settings().setAcceleratedCompositingForFixedPositionEnabled(s_tiledCompositingEnabled);
        m_page->settings().setAcceleratedCompositingEnabled(true);
#if ENABLE(VIDEO)
        MediaPlayerPrivateSymbian::setCompositingActive(true);
#endif
        invalidate(IntRect(IntPoint(), m_size));
        return true;
    }
#else
    UNUSED_PARAM(window);
#endif
    return false;
}

bool WebViewImpl::allowsAcceleratedCompositing() const
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    return !!m_nativeWindow;
#else
    return false;
#endif
}

bool WebViewImpl::wantsCompositing() const
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    return m_nativeWindow && (m_rootLayer || m_overlayLayer);
#else
    return false;
#endif
}

bool WebViewImpl::usesAcceleratedCompositing() const
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    return m_compositor || wantsCompositing();
#else
    return false;
#endif
}

void WebViewImpl::disableCompositing()
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    m_nativeWindow = nullptr;
    m_compositorIdleTimer.stop();
    m_compositorIdle = false;
#if ENABLE(VIDEO)
    MediaPlayerPrivateSymbian::setCompositingActive(false);
#endif
    setVideoUnderlayVisible(false);
    if (!m_page->settings().acceleratedCompositingEnabled() && !m_compositor)
        return;
    if (m_compositor)
        m_compositor->prepareForDestruction();
    m_page->settings().setForceCompositingMode(false);
    m_page->settings().setAcceleratedCompositingEnabled(false);
    // Disabling the setting tears down RenderLayer backings on the next
    // style/layout pass. Keep their parent/context alive for that pass.
    if (auto* view = m_page->localMainFrame()->view()) {
        view->updateLayoutAndStyleIfNeededRecursive();
        view->flushCompositingStateIncludingSubframes();
    }
    m_compositor = nullptr;
    m_rootLayer = nullptr;
    m_overlayLayer = nullptr;
    invalidate(IntRect(IntPoint(), m_size));
#endif
}

void WebViewImpl::renderCompositedFrame()
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    if (!usesAcceleratedCompositing())
        return;
#if ENABLE(VIDEO)
    // Without a video window of the embedding's own under the page, MMF
    // takes the page window's background surface, which EGL uses: paint the
    // page in software, with holes for the video.
    if (!MediaPlayerPrivateSymbian::compositedVideoEnabled() && MediaPlayerPrivateSymbian::requiresNativeVideoWindow()) {
        WTFLogAlways("Symbian compositor: native video window requires software painting");
        disableCompositing();
        return;
    }
#endif
    if (wantsCompositing()) {
        m_compositorIdle = false;
        m_compositorIdleTimer.stop();
    } else if (!m_compositorIdle) {
        // Navigations often leave and re-enter compositing; keep EGL briefly.
        m_compositorIdle = true;
        m_compositorIdleTimer.startOneShot(compositorIdleTimeout);
    } else if (!m_compositorIdleTimer.isActive()) {
        WTFLogAlways("Symbian compositor: no composited layers, EGL composition ended");
        m_compositorIdle = false;
        setVideoUnderlayVisible(false);
        m_compositor->prepareForDestruction();
        m_compositor = nullptr;
        // The embedding presents the whole view itself again.
        m_client.didInvalidate({ 0, 0, m_size.width(), m_size.height() });
        return;
    }
    if (!m_compositor) {
        MonotonicTime start = MonotonicTime::now();
        m_compositor = SymbianCompositor::create(*this, m_nativeWindow);
        if (!m_compositor) {
            disableCompositing();
            return;
        }
        m_compositor->setRootLayer(m_rootLayer.get());
        m_compositor->setOverlayLayer(m_overlayLayer.get());
        WTFLogAlways("Symbian compositor: EGL composition started in %.1f ms", (MonotonicTime::now() - start).milliseconds());
    }
    if (!m_compositor->render())
        disableCompositing();
#endif
}

bool WebViewImpl::requiresNativeVideoWindow() const
{
#if ENABLE(VIDEO)
    return MediaPlayerPrivateSymbian::requiresNativeVideoWindow();
#else
    return false;
#endif
}

void WebViewImpl::attachRootGraphicsLayer(GraphicsLayer* layer)
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    bool starts = layer && !usesAcceleratedCompositing();
    m_rootLayer = layer;
    if (m_compositor)
        m_compositor->setRootLayer(layer);
    else if (starts)
        requestCompositedDisplay();
#else
    UNUSED_PARAM(layer);
#endif
}

void WebViewImpl::attachViewOverlayGraphicsLayer(GraphicsLayer* layer)
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    bool starts = layer && !usesAcceleratedCompositing();
    m_overlayLayer = layer;
    if (m_compositor)
        m_compositor->setOverlayLayer(layer);
    else if (starts)
        requestCompositedDisplay();
#else
    UNUSED_PARAM(layer);
#endif
}

void WebViewImpl::setVideoUnderlayVisible(bool visible)
{
    if (m_videoUnderlayVisible == visible)
        return;
    m_videoUnderlayVisible = visible;
    WTFLogAlways("video: holes in composited frames %s", visible ? "shown" : "gone");
    m_client.setVideoUnderlayVisible(visible);
}

void WebViewImpl::requestCompositedDisplay()
{
    m_client.didInvalidate({ 0, 0, m_size.width(), m_size.height() });
}

void WebViewImpl::scheduleRenderingUpdate()
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    if (m_compositor) {
        m_compositor->scheduleRenderingUpdate();
        return;
    }
#endif
    m_page->isolatedUpdateRendering();
}

LocalFrame& WebViewImpl::focusedFrame() const
{
    return *m_page->focusController().focusedOrMainFrame();
}

void WebViewImpl::invalidate(const IntRect& rect)
{
    IntRect visible = intersection(rect, IntRect(IntPoint(), m_size));
    if (visible.isEmpty())
        return;
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    if (m_compositor)
        m_compositor->invalidate(visible);
#endif
    m_client.didInvalidate({ visible.x(), visible.y(), visible.width(), visible.height() });
}

void WebViewImpl::scroll(const IntSize& delta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    IntRect area = intersection(intersection(rectToScroll, clipRect), IntRect(IntPoint(), m_size));
    if (area.isEmpty())
        return;
    if (usesAcceleratedCompositing() || std::abs(delta.width()) >= area.width()
        || std::abs(delta.height()) >= area.height()
        || !m_client.didScroll({ area.x(), area.y(), area.width(), area.height() }, delta.width(), delta.height())) {
        noteScrollPath(ScrollPath::Declined);
        invalidate(area);
        return;
    }
    noteScrollPath(ScrollPath::Blit);
    // The client moved the pixels that stay visible; paint what scrolled in.
    IntRect kept = area;
    kept.move(delta);
    kept.intersect(area);
    if (kept.y() > area.y())
        invalidate({ area.x(), area.y(), area.width(), kept.y() - area.y() });
    if (kept.maxY() < area.maxY())
        invalidate({ area.x(), kept.maxY(), area.width(), area.maxY() - kept.maxY() });
    if (kept.x() > area.x())
        invalidate({ area.x(), kept.y(), kept.x() - area.x(), kept.height() });
    if (kept.maxX() < area.maxX())
        invalidate({ kept.maxX(), kept.y(), area.maxX() - kept.maxX(), kept.height() });
}

void WebViewImpl::noteScrollPath(ScrollPath path)
{
    if (!frameStatsEnabled())
        return;
    unsigned count = ++m_scrollPaths[static_cast<int>(path)];
    if (count != 1 && count % 100)
        return;
    auto* view = m_page->localMainFrame() ? m_page->localMainFrame()->view() : nullptr;
    WTFLogAlways("scroll: blit=%u declined=%u slow=%u (canBlit=%d slowObjects=%d fixed=%d transparent=%d compositing=%d)",
        m_scrollPaths[0], m_scrollPaths[1], m_scrollPaths[2],
        view && view->canBlitOnScroll(), view && view->hasSlowRepaintObjects(),
        view && view->hasViewportConstrainedObjects(), view && view->isTransparent(),
        usesAcceleratedCompositing());
}

void WebViewImpl::focusedElementChanged()
{
    m_client.didChangeEditingState();
}

void WebViewImpl::selectionChanged()
{
    m_client.didChangeSelection();
}

void WebViewImpl::titleChanged(const String& title)
{
    m_client.didChangeTitle(UTF16Text(title).text());
}

void WebViewImpl::urlChanged()
{
    DocumentLoader* loader = m_page->localMainFrame()->loader().documentLoader();
    if (!loader)
        return;
    URL url = loader->unreachableURL().isEmpty() ? loader->url() : loader->unreachableURL();
    m_client.didChangeURL(UTF16Text(url.string()).text());
}

void WebViewImpl::progressChanged()
{
    m_client.didChangeProgress(m_page->localMainFrame()->loader().isLoading(), m_page->progress().estimatedProgress());
}

void WebViewImpl::loadErrorPage(const ResourceError& error)
{
    WTFLogAlways("load failed: %s: %s (%d)", error.failingURL().string().utf8().data(),
        error.localizedDescription().utf8().data(), error.errorCode());
    StringBuilder html;
    html.append("<!DOCTYPE html><meta name=viewport content='width=device-width'>"
        "<title>Problem loading page</title><body style='font-family:sans-serif;margin:16px'>"
        "<h3>This page could not be loaded</h3><p>"_s);
    html.append(error.localizedDescription().isEmpty() ? "Unknown error"_s : error.localizedDescription());
    html.append("</p><p style='word-wrap:break-word;color:#666'>"_s);
    html.append(error.failingURL().string());
    html.append("</p></body>"_s);
    CString utf8 = html.toString().utf8();
    URL failingURL = error.failingURL();
    ResourceResponse response(URL { failingURL }, "text/html"_s, utf8.length(), "UTF-8"_s);
    SubstituteData substituteData(SharedBuffer::create(utf8.span()), URL { failingURL }, WTF::move(response), SubstituteData::SessionHistoryVisibility::Hidden);
    LocalFrame& frame = *m_page->localMainFrame();
    frame.loader().load(FrameLoadRequest(frame, ResourceRequest(WTF::move(failingURL)), WTF::move(substituteData)));
}

void WebViewImpl::setSize(int width, int height)
{
    bool widthChanged = width != m_size.width();
    m_size = IntSize(width, height);
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    if (m_compositor && !m_compositor->resize())
        disableCompositing();
#endif
    if (LocalFrameView* view = m_page->localMainFrame()->view())
        view->resize(m_size);
    // The split view keyboard changes only the height, and keeps the zoom.
    if (widthChanged)
        updateLayoutSize();
    invalidate(IntRect(IntPoint(), m_size));
}

// Viewport.

static const int desktopWidth = 980;

void WebViewImpl::viewportChanged(const ViewportArguments& arguments)
{
    // Each new document starts with implicit arguments, before its
    // <meta name=viewport> if it has one.
    m_viewportArguments = arguments;
    updateLayoutSize();
}

ViewportAttributes WebViewImpl::viewportAttributes() const
{
    ViewportAttributes attributes = computeViewportAttributes(m_viewportArguments, desktopWidth, m_size.width(), m_size.height(), 1, m_size);
    restrictMinimumScaleFactorToViewportSize(attributes, m_size, 1);
    restrictScaleFactorToInitialScaleIfNotUserScalable(attributes);
    attributes.initialScale = clampTo(attributes.initialScale, attributes.minimumScale, attributes.maximumScale);
    return attributes;
}

void WebViewImpl::updateLayoutSize()
{
    LocalFrameView* view = m_page->localMainFrame()->view();
    if (!view || m_size.isEmpty())
        return;
    ViewportAttributes attributes = viewportAttributes();
    view->setFixedLayoutSize(roundedIntSize(attributes.layoutSize));
    view->setUseFixedLayout(true);
    m_initialScale = attributes.initialScale;
    m_minimumScale = attributes.minimumScale;
    m_maximumScale = attributes.maximumScale;
    // Scaling restyles the whole page, so it waits until the document that
    // asked for it has settled in.
    m_scaleTimer.startOneShot(0_s);
}

void WebViewImpl::updateScale()
{
    if (m_page->localMainFrame()->document())
        m_page->setPageScaleFactor(m_initialScale, IntPoint());
}

// Toggles between the initial scale and one where text is readable, keeping
// the point under the finger.
void WebViewImpl::zoomAt(const IntPoint& point)
{
    LocalFrameView* view = m_page->localMainFrame()->view();
    if (!view || !m_page->localMainFrame()->document())
        return;
    float scale = m_page->pageScaleFactor();
    float zoomed = scale > m_initialScale * 1.01f ? m_initialScale : std::max(1.f, m_initialScale * 2);
    zoomed = clampTo(zoomed, m_minimumScale, m_maximumScale);
    if (zoomed == scale)
        return;
    FloatPoint documentPoint = view->scrollPosition() + toIntSize(point);
    documentPoint.scale(zoomed / scale);
    m_page->setPageScaleFactor(zoomed, roundedIntPoint(documentPoint) - toIntSize(point));
}

void WebViewImpl::paint(uint8_t* pixels, int stride, const Rect& area)
{
    IntRect rect = intersection(IntRect(area.x, area.y, area.width, area.height), IntRect(IntPoint(), m_size));
    if (rect.isEmpty())
        return;
#if ENABLE(VIDEO)
    MediaPlayerPrivateSymbian::willPaint();
#endif
    cairo_surface_t* surface = cairo_image_surface_create_for_data(pixels, CAIRO_FORMAT_ARGB32, m_size.width(), m_size.height(), stride);
    {
        GraphicsContextCairo context(surface);
        context.clip(rect);
        LocalFrameView* view = m_page->localMainFrame()->view();
        if (view) {
            view->updateLayoutAndStyleIfNeededRecursive();
            view->paint(context, rect);
        } else
            context.fillRect(rect, Color::white);
    }
    cairo_surface_flush(surface);
    cairo_surface_destroy(surface);
#if ENABLE(VIDEO)
    MediaPlayerPrivateSymbian::didPaint(rect);
#endif
}

void WebViewImpl::updateLayout()
{
    if (LocalFrameView* view = m_page->localMainFrame()->view())
        view->updateLayoutAndStyleIfNeededRecursive();
}

void WebViewImpl::load(const char* url)
{
    String string = String::fromUTF8(url).trim(isASCIIWhitespace);
    if (string.isEmpty())
        return;
    // Phone paths (C:\Data\page.html) are files, not URLs with a "c" scheme.
    if (string.length() > 2 && isASCIIAlpha(string[0]) && string[1] == ':' && (string[2] == '/' || string[2] == '\\'))
        string = makeString("file:///"_s, makeStringByReplacingAll(string, '\\', '/'));
    URL parsed(URL(), string);
    if (!parsed.isValid() || parsed.protocol().isEmpty() || !string.contains(':'))
        parsed = URL(URL(), makeString("http://"_s, string));
    m_client.cancelAuthenticationChallenge(0);
    LocalFrame& frame = *m_page->localMainFrame();
    frame.loader().load(FrameLoadRequest(frame, ResourceRequest(WTF::move(parsed))));
}

void WebViewImpl::reload()
{
    m_client.cancelAuthenticationChallenge(0);
    m_page->localMainFrame()->loader().reload();
}

void WebViewImpl::stop()
{
    m_client.cancelAuthenticationChallenge(0);
    m_page->localMainFrame()->loader().stopAllLoaders();
}

void WebViewImpl::goBack()
{
    m_client.cancelAuthenticationChallenge(0);
    m_page->backForward().goBack();
}

void WebViewImpl::goForward()
{
    m_client.cancelAuthenticationChallenge(0);
    m_page->backForward().goForward();
}

bool WebViewImpl::canGoBack() const
{
    return m_page->backForward().canGoBackOrForward(-1);
}

bool WebViewImpl::canGoForward() const
{
    return m_page->backForward().canGoBackOrForward(1);
}

void WebViewImpl::setMediaPlaybackRequiresUserGesture(bool required)
{
    // Media sessions read the restriction when an element or context is made.
    m_page->settings().setRequiresUserGestureForAudioPlayback(required);
}

void WebViewImpl::setFocused(bool focused)
{
    if (!focused) {
        cancelTouches();
        PlatformKeyboardEvent::setCurrentModifierState({ });
    }
    m_page->focusController().setActive(true);
    m_page->focusController().setFocused(focused);
}

static const int tapSlop = 8;

#if ENABLE(TOUCH_EVENTS)
class SymbianPlatformTouchPoint final : public PlatformTouchPoint {
public:
    explicit SymbianPlatformTouchPoint(const TouchPoint& point)
    {
        m_id = point.identifier;
        m_isPrimary = point.isPrimary;
        m_pos = DoublePoint(point.x, point.y);
        m_screenPos = DoublePoint(point.screenX, point.screenY);
        m_radius = { 0.5, 0.5 };
        m_force = point.pressure;
        m_twist = 0;
        switch (point.state) {
        case TouchState::Pressed: m_state = TouchPressed; break;
        case TouchState::Moved: m_state = TouchMoved; break;
        case TouchState::Released: m_state = TouchReleased; break;
        case TouchState::Stationary: m_state = TouchStationary; break;
        case TouchState::Cancelled: m_state = TouchCancelled; break;
        }
    }
};

class SymbianPlatformTouchEvent final : public PlatformTouchEvent {
public:
    SymbianPlatformTouchEvent(const TouchPoint* points, unsigned count, unsigned modifiers)
    {
        m_timestamp = MonotonicTime::now();
        m_type = PlatformEventType::TouchMove;
        if (modifiers & ShiftKey)
            m_modifiers.add(Modifier::ShiftKey);
        if (modifiers & ControlKey)
            m_modifiers.add(Modifier::ControlKey);
        if (modifiers & AltKey)
            m_modifiers.add(Modifier::AltKey);
        for (unsigned i = 0; i < count; ++i) {
            m_touchPoints.append(SymbianPlatformTouchPoint(points[i]));
            switch (points[i].state) {
            case TouchState::Pressed: m_type = PlatformEventType::TouchStart; break;
            case TouchState::Released: m_type = PlatformEventType::TouchEnd; break;
            case TouchState::Cancelled: m_type = PlatformEventType::TouchCancel; break;
            default: break;
            }
        }
    }
};
#endif

unsigned WebViewImpl::setTouchCapabilities(unsigned capacity)
{
    cancelTouches();
#if ENABLE(TOUCH_EVENTS)
    m_touchCapacity = std::min(capacity, maximumSupportedTouchPoints);
#else
    UNUSED_PARAM(capacity);
    m_touchCapacity = 0;
#endif
    return m_touchCapacity;
}

void WebViewImpl::cancelDOMTouches()
{
#if ENABLE(TOUCH_EVENTS)
    if (m_touchDOMCancelled || m_touchPoints.isEmpty())
        return;
    auto generation = m_touchGeneration;
    m_touchDOMCancelled = true;
    auto cancelled = m_touchPoints;
    for (auto& point : cancelled) {
        point.state = TouchState::Cancelled;
        point.pressure = 0;
    }
    if (RefPtr frame = m_page->localMainFrame(); frame && frame->view())
        frame->eventHandler().handleTouchEvent(SymbianPlatformTouchEvent(cancelled.span().data(), cancelled.size(), 0));
    if (generation != m_touchGeneration)
        return;
    for (auto& point : cancelled)
        m_page->pointerCaptureController().touchWithIdentifierWasRemoved(mousePointerID + 1 + point.identifier);
#endif
}

void WebViewImpl::cancelTouches()
{
    auto generation = ++m_touchGeneration;
    auto cancelled = std::exchange(m_touchPoints, { });
    bool alreadyCancelled = std::exchange(m_touchDOMCancelled, false);
    m_touchGesture.reset();
    m_pointerDown = false;
    m_pointerScrolling = false;
    m_lastTapWasOnFocusedEditableElement = false;
    m_client.didCancelTouchSequence();
#if ENABLE(TOUCH_EVENTS)
    if (!alreadyCancelled && !cancelled.isEmpty()) {
        for (auto& point : cancelled) {
            point.state = TouchState::Cancelled;
            point.pressure = 0;
        }
        if (RefPtr frame = m_page->localMainFrame(); frame && frame->view())
            frame->eventHandler().handleTouchEvent(SymbianPlatformTouchEvent(cancelled.span().data(), cancelled.size(), 0));
    }
    if (generation == m_touchGeneration) {
        for (auto& point : cancelled)
            m_page->pointerCaptureController().touchWithIdentifierWasRemoved(mousePointerID + 1 + point.identifier);
    }
#else
    UNUSED_PARAM(generation);
    UNUSED_PARAM(cancelled);
    UNUSED_PARAM(alreadyCancelled);
#endif
}

bool WebViewImpl::handleTouch(const TouchPoint* points, unsigned count, unsigned modifiers)
{
#if ENABLE(TOUCH_EVENTS)
    if (!m_touchCapacity || !points || !count || count > m_touchCapacity)
        return false;
    std::array<bool, maximumSupportedTouchPoints> identifiers { };
    bool fresh = m_touchPoints.isEmpty();
    bool allReleased = true;
    bool hasPressed = false;
    bool cancelled = false;
    for (unsigned i = 0; i < count; ++i) {
        const auto& point = points[i];
        if (point.identifier >= m_touchCapacity || identifiers[point.identifier]) {
            cancelTouches();
            return false;
        }
        identifiers[point.identifier] = true;
        hasPressed |= point.state == TouchState::Pressed;
        cancelled |= point.state == TouchState::Cancelled;
        if (point.state != TouchState::Released && point.state != TouchState::Cancelled)
            allReleased = false;
    }
    if (cancelled) {
        cancelTouches();
        return false;
    }
    if (fresh && !hasPressed)
        return false;
    RefPtr frame = m_page->localMainFrame();
    if (!frame || !frame->view()) {
        cancelTouches();
        return false;
    }

    if (fresh) {
        m_lastTapWasOnFocusedEditableElement = false;
        ++m_touchGeneration;
        m_touchDOMCancelled = false;
        m_touchAllowsDoubleTap = true;
        m_touchGesture.reset();
        auto pagePoint = frame->view()->windowToContents(IntPoint(points[0].x, points[0].y));
        auto hit = frame->eventHandler().hitTestResultAtPoint(pagePoint, { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::TouchEvent, HitTestRequest::Type::AllowChildFrameContent });
        if (RefPtr node = hit.innerNode(); node && node->renderer()) {
            auto actions = Style::toPlatform(node->renderer()->style().usedTouchAction());
            bool unrestrictedPan = actions.contains(TouchAction::Auto) || actions.contains(TouchAction::Manipulation);
            m_touchGesture.setPanDirections(unrestrictedPan || actions.contains(TouchAction::PanX), unrestrictedPan || actions.contains(TouchAction::PanY));
            m_touchAllowsDoubleTap = actions.contains(TouchAction::Auto);
        }
    }
    auto generation = m_touchGeneration;
    m_touchPoints.clear();
    for (unsigned i = 0; i < count; ++i) {
        if (points[i].state != TouchState::Released) {
            auto point = points[i];
            point.state = TouchState::Stationary;
            m_touchPoints.append(point);
        }
    }
    bool prevented = false;
    bool suppressCompatibilityMouse = false;
    if (!m_touchDOMCancelled) {
        auto result = frame->eventHandler().handleTouchEvent(SymbianPlatformTouchEvent(points, count, modifiers), &prevented);
        suppressCompatibilityMouse = result.value_or(true);
        if (!result)
            prevented = true;
    }
    // DOM handlers may synchronously navigate or move native focus.
    if (generation != m_touchGeneration)
        return false;
    if (prevented || suppressCompatibilityMouse || count > 1)
        m_lastTapTime = { };
    auto action = m_touchGesture.update(points, count, prevented, suppressCompatibilityMouse);
    bool tapped = false;
    if (action.action == TouchGesture::Action::BeginScroll)
        cancelDOMTouches();
    if (generation != m_touchGeneration)
        return false;
    if (action.action == TouchGesture::Action::BeginScroll || action.action == TouchGesture::Action::Scroll)
        scrollAt({ action.x, action.y }, { action.deltaX, action.deltaY });
    else if (action.action == TouchGesture::Action::Tap) {
        tapped = tap({ action.x, action.y }, action.identifier, true);
    }
    if (generation != m_touchGeneration)
        return false;
    for (unsigned i = 0; i < count; ++i) {
        if (points[i].state == TouchState::Released)
            m_page->pointerCaptureController().touchWithIdentifierWasRemoved(mousePointerID + 1 + points[i].identifier);
    }
    if (allReleased)
        m_touchDOMCancelled = false;
    return tapped;
#else
    UNUSED_PARAM(points);
    UNUSED_PARAM(count);
    UNUSED_PARAM(modifiers);
    return false;
#endif
}

void WebViewImpl::handlePointer(PointerEvent event, int x, int y)
{
    PlatformKeyboardEvent::setCurrentModifierState({ });
    IntPoint point(x, y);
    switch (event) {
    case PointerEvent::Down:
        m_lastTapWasOnFocusedEditableElement = false;
        m_pointerDown = true;
        m_pointerScrolling = false;
        m_pointerStart = m_pointerLast = point;
        break;
    case PointerEvent::Move:
        if (!m_pointerDown)
            break;
        if (!m_pointerScrolling) {
            IntSize moved = point - m_pointerStart;
            m_pointerScrolling = std::abs(moved.width()) > tapSlop || std::abs(moved.height()) > tapSlop;
        }
        if (m_pointerScrolling) {
            scrollAt(point, point - m_pointerLast);
            m_pointerLast = point;
        }
        break;
    case PointerEvent::Up:
        if (m_pointerDown && !m_pointerScrolling)
            tap(m_pointerStart);
        m_pointerDown = false;
        break;
    }
}

static const Seconds doubleTapInterval = 400_ms;
static const int doubleTapSlop = 24;

RefPtr<Element> WebViewImpl::editableElementAt(const IntPoint& point) const
{
    RefPtr frame = m_page->localMainFrame();
    if (!frame || !frame->view())
        return nullptr;
    auto pagePoint = frame->view()->windowToContents(point);
    auto hit = frame->eventHandler().hitTestResultAtPoint(pagePoint, { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowChildFrameContent });
    // Text controls hit their UA shadow editor; compare the public control
    // with the focused element, not that internal editor's selection root.
    hit.setToNonUserAgentShadowAncestor();
    RefPtr node = hit.innerNode();
    if (!node)
        return nullptr;
    for (RefPtr current = node; current; current = current->parentNode()) {
        if (auto* input = dynamicDowncast<HTMLInputElement>(*current))
            return !input->isDisabledFormControl() && !input->isReadOnly() && input->isTextField() ? input : nullptr;
        if (auto* textArea = dynamicDowncast<HTMLTextAreaElement>(*current))
            return !textArea->isDisabledFormControl() && !textArea->isReadOnly() ? textArea : nullptr;
        if (is<HTMLFormControlElement>(*current))
            return nullptr;
    }
    return node->rootEditableElement();
}

bool WebViewImpl::tap(const IntPoint& point, unsigned touchIdentifier, bool fromTouch)
{
    m_lastTapWasOnFocusedEditableElement = false;
    MonotonicTime now = MonotonicTime::now();
    IntSize moved = point - m_lastTap;
    bool doubleTap = now - m_lastTapTime < doubleTapInterval
        && std::abs(moved.width()) <= doubleTapSlop && std::abs(moved.height()) <= doubleTapSlop;
    m_lastTap = point;
    // A third tap starts over.
    m_lastTapTime = doubleTap ? MonotonicTime() : now;
    // The first tap has clicked already; the second zooms instead, on pages
    // that can zoom.
    if (doubleTap && (!fromTouch || m_touchAllowsDoubleTap) && m_maximumScale > m_minimumScale) {
        zoomAt(point);
        return false;
    }
    auto generation = m_touchGeneration;
    RefPtr tappedEditable = click(point, touchIdentifier, fromTouch);
    if (generation == m_touchGeneration && tappedEditable && tappedEditable->isConnected() && focusedInputType() != InputType::None) {
        auto* document = focusedFrame().document();
        RefPtr focused = document ? document->focusedElement() : nullptr;
        if (focused && (is<HTMLInputElement>(*focused) || is<HTMLTextAreaElement>(*focused)))
            m_lastTapWasOnFocusedEditableElement = tappedEditable == focused;
        else
            m_lastTapWasOnFocusedEditableElement = tappedEditable.get() == editableRoot();
    }
    return true;
}

RefPtr<Element> WebViewImpl::click(const IntPoint& point, unsigned touchIdentifier, bool fromTouch)
{
    auto& handler = m_page->localMainFrame()->eventHandler();
    MonotonicTime now = MonotonicTime::now();
    class CompatibilityMouseEvent : public PlatformMouseEvent {
    public:
        CompatibilityMouseEvent(const IntPoint& point, MouseButton button, Type type, MonotonicTime now, unsigned identifier, bool touch)
            : PlatformMouseEvent(point, point, button, type, button == MouseButton::None ? 0 : 1, { }, now,
                button == MouseButton::None ? 0 : ForceAtClick,
                touch || button != MouseButton::None ? SyntheticClickType::OneFingerTap : SyntheticClickType::NoTap,
                touch ? mousePointerID + 1 + identifier : mousePointerID)
        {
            if (touch)
                m_pointerType = touchPointerEventType();
        }
    };
    handler.mouseMoved(CompatibilityMouseEvent(point, MouseButton::None, PlatformEvent::Type::MouseMoved, now, touchIdentifier, fromTouch));
    // Mouse-over handlers have run; retain the actual press hit before down/
    // click handlers can redirect focus or change the layout.
    RefPtr tappedEditable = editableElementAt(point);
    handler.handleMousePressEvent(CompatibilityMouseEvent(point, MouseButton::Left, PlatformEvent::Type::MousePressed, now, touchIdentifier, fromTouch));
    handler.handleMouseReleaseEvent(CompatibilityMouseEvent(point, MouseButton::Left, PlatformEvent::Type::MouseReleased, now, touchIdentifier, fromTouch));
    return tappedEditable;
}

void WebViewImpl::scrollAt(const IntPoint& point, const IntSize& delta)
{
    PlatformKeyboardEvent::setCurrentModifierState({ });
    // A wheel event scrolls whatever is under the finger, overflow areas
    // included; the content follows the finger.
    PlatformWheelEvent event(point, point, delta.width(), delta.height(), 0, 0, PlatformWheelEventGranularity::ScrollByPixelWheelEvent, false, false, false, false);
    event.setHasPreciseScrollingDeltas(true);
    m_page->localMainFrame()->eventHandler().handleWheelEvent(event, { WheelEventProcessingSteps::SynchronousScrolling, WheelEventProcessingSteps::BlockingDOMEventDispatch });
}

void WebViewImpl::scrollBy(int dx, int dy)
{
    if (LocalFrameView* view = m_page->localMainFrame()->view())
        view->scrollBy(IntSize(dx, dy));
}

bool WebViewImpl::handleKey(bool isDown, int keyCode, Text text, unsigned modifiers)
{
    OptionSet<PlatformEvent::Modifier> eventModifiers;
    if (modifiers & ShiftKey)
        eventModifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (modifiers & ControlKey)
        eventModifiers.add(PlatformEvent::Modifier::ControlKey);
    if (modifiers & AltKey)
        eventModifiers.add(PlatformEvent::Modifier::AltKey);
    PlatformKeyboardEvent::setCurrentModifierState(eventModifiers);
    String characters = toString(text);
    String key = keyForKeyCode(keyCode, characters);
    String keyIdentifier = keyIdentifierForKeyCode(keyCode, characters);
    MonotonicTime now = MonotonicTime::now();
    auto& handler = focusedFrame().eventHandler();
    if (!isDown)
        return handler.keyEvent(PlatformKeyboardEvent(PlatformEvent::Type::KeyUp, { }, { }, key, { }, keyIdentifier, keyCode, false, false, false, eventModifiers, now));

    // Text also carries the DOM key identity. Control shortcuts must not turn
    // into text insertion when the page/editor leaves their keydown unhandled.
    // Keep printable Ctrl+Alt (AltGr) output supplied by the native input method.
    bool handled = handler.keyEvent(PlatformKeyboardEvent(PlatformEvent::Type::RawKeyDown, { }, { }, key, { }, keyIdentifier, keyCode, false, false, false, eventModifiers, now));
    if (handled || characters.isEmpty()
        || ((modifiers & ControlKey) && (!(modifiers & AltKey) || characters[0] < ' ')))
        return handled;
    return focusedFrame().eventHandler().keyEvent(PlatformKeyboardEvent(PlatformEvent::Type::Char, characters, characters, key, { }, keyIdentifier, keyCode, false, false, false, eventModifiers, now));
}

// Editing commands for keys, as the Windows port maps them.

namespace {

const unsigned CtrlKeyFlag = 1 << 0;
const unsigned AltKeyFlag = 1 << 1;
const unsigned ShiftKeyFlag = 1 << 2;

struct KeyDownEntry {
    unsigned virtualKey;
    unsigned modifiers;
    const char* name;
};

struct KeyPressEntry {
    unsigned charCode;
    unsigned modifiers;
    const char* name;
};

const KeyDownEntry keyDownEntries[] = {
    { VK_LEFT, 0, "MoveLeft" },
    { VK_LEFT, ShiftKeyFlag, "MoveLeftAndModifySelection" },
    { VK_RIGHT, 0, "MoveRight" },
    { VK_RIGHT, ShiftKeyFlag, "MoveRightAndModifySelection" },
    { VK_UP, 0, "MoveUp" },
    { VK_UP, ShiftKeyFlag, "MoveUpAndModifySelection" },
    { VK_DOWN, 0, "MoveDown" },
    { VK_DOWN, ShiftKeyFlag, "MoveDownAndModifySelection" },
    { VK_PRIOR, 0, "MovePageUp" },
    { VK_NEXT, 0, "MovePageDown" },
    { VK_HOME, 0, "MoveToBeginningOfLine" },
    { VK_HOME, ShiftKeyFlag, "MoveToBeginningOfLineAndModifySelection" },
    { VK_END, 0, "MoveToEndOfLine" },
    { VK_END, ShiftKeyFlag, "MoveToEndOfLineAndModifySelection" },
    { VK_BACK, 0, "DeleteBackward" },
    { VK_BACK, ShiftKeyFlag, "DeleteBackward" },
    { VK_DELETE, 0, "DeleteForward" },
    { VK_ESCAPE, 0, "Cancel" },
    { VK_TAB, 0, "InsertTab" },
    { VK_TAB, ShiftKeyFlag, "InsertBacktab" },
    { VK_RETURN, 0, "InsertNewline" },
    { VK_RETURN, ShiftKeyFlag, "InsertNewline" },
    { 'C', CtrlKeyFlag, "Copy" },
    { 'V', CtrlKeyFlag, "Paste" },
    { 'X', CtrlKeyFlag, "Cut" },
    { 'A', CtrlKeyFlag, "SelectAll" },
};

const KeyPressEntry keyPressEntries[] = {
    { '\t', 0, "InsertTab" },
    { '\t', ShiftKeyFlag, "InsertBacktab" },
    { '\r', 0, "InsertNewline" },
    { '\r', ShiftKeyFlag, "InsertNewline" },
};

const char* interpretKeyEvent(const KeyboardEvent& event)
{
    unsigned modifiers = 0;
    if (event.shiftKey())
        modifiers |= ShiftKeyFlag;
    if (event.altKey())
        modifiers |= AltKeyFlag;
    if (event.ctrlKey())
        modifiers |= CtrlKeyFlag;

    if (event.type() == eventNames().keydownEvent) {
        for (auto& entry : keyDownEntries) {
            if (entry.virtualKey == static_cast<unsigned>(event.keyCode()) && entry.modifiers == modifiers)
                return entry.name;
        }
        return nullptr;
    }
    for (auto& entry : keyPressEntries) {
        if (entry.charCode == static_cast<unsigned>(event.charCode()) && entry.modifiers == modifiers)
            return entry.name;
    }
    return nullptr;
}

} // namespace

bool WebViewImpl::handleEditingKeyboardEvent(KeyboardEvent& event)
{
    auto* frame = downcast<Node>(event.target())->document().frame();
    auto* keyEvent = event.underlyingPlatformEvent();
    if (!frame || !keyEvent || keyEvent->isSystemKey())
        return false;

    auto command = frame->editor().command(String::fromLatin1(interpretKeyEvent(event)));
    if (keyEvent->type() == PlatformEvent::Type::RawKeyDown) {
        // Commands that insert text wait for the key press, so the page sees it.
        return !command.isTextInsertion() && command.execute(&event);
    }
    if (command.execute(&event))
        return true;
    if (event.charCode() < ' ')
        return false;
    return frame->editor().insertText(keyEvent->text(), &event);
}

namespace {
String editCommandName(EditCommand command)
{
    switch (command) {
    case EditCommand::Cut: return "Cut"_s;
    case EditCommand::Copy: return "Copy"_s;
    case EditCommand::Paste: return "Paste"_s;
    case EditCommand::SelectAll: return "SelectAll"_s;
    }
    return { };
}
}

bool WebViewImpl::canExecuteEditCommand(EditCommand command) const
{
    auto name = editCommandName(command);
    return !name.isNull() && focusedFrame().editor().command(name).isEnabled();
}

bool WebViewImpl::executeEditCommand(EditCommand command)
{
    auto name = editCommandName(command);
    if (name.isNull())
        return false;
    Ref frame = focusedFrame();
    UserGestureIndicator gesture(IsProcessingUserGesture::Yes, frame->document());
    return frame->editor().command(name).execute();
}

// Editing for the input method. Offsets count characters of the root
// editable element: the inner text of an <input> or <textarea>, or the
// contenteditable element.

Element* WebViewImpl::editableRoot() const
{
    return focusedFrame().selection().selection().rootEditableElement();
}

InputType WebViewImpl::focusedInputType() const
{
    auto* document = focusedFrame().document();
    Element* focused = document ? document->focusedElement() : nullptr;
    if (!focused)
        return InputType::None;
    if (is<HTMLInputElement>(*focused)) {
        auto& input = downcast<HTMLInputElement>(*focused);
        if (input.isDisabledFormControl() || input.isReadOnly() || !input.isTextField())
            return InputType::None;
        if (input.isPasswordField())
            return InputType::Password;
        if (input.isNumberField())
            return InputType::Number;
        if (input.isTelephoneField())
            return InputType::Telephone;
        if (input.isEmailField())
            return InputType::Email;
        if (input.isURLField())
            return InputType::URL;
        if (input.isSearchField())
            return InputType::Search;
        return InputType::Text;
    }
    if (is<HTMLTextAreaElement>(*focused)) {
        auto& textArea = downcast<HTMLTextAreaElement>(*focused);
        return textArea.isDisabledFormControl() || textArea.isReadOnly() ? InputType::None : InputType::MultiLine;
    }
    return editableRoot() ? InputType::MultiLine : InputType::None;
}

int WebViewImpl::editableTextLength() const
{
    Element* root = editableRoot();
    return root ? characterCount(makeRangeSelectingNodeContents(*root)) : 0;
}

int WebViewImpl::maximumEditableTextLength() const
{
    auto* document = focusedFrame().document();
    Element* focused = document ? document->focusedElement() : nullptr;
    if (is<HTMLInputElement>(focused)) {
        int maxLength = downcast<HTMLInputElement>(*focused).effectiveMaxLength();
        if (maxLength > 0)
            return maxLength;
    }
    return 1 << 20;
}

int WebViewImpl::copyEditableText(int start, int length, uint16_t* buffer) const
{
    Element* root = editableRoot();
    if (!root || start < 0 || length <= 0)
        return 0;
    String text = plainText(makeRangeSelectingNodeContents(*root));
    if (static_cast<unsigned>(start) >= text.length())
        return 0;
    unsigned count = std::min<unsigned>(length, text.length() - start);
    for (unsigned i = 0; i < count; ++i)
        buffer[i] = text[start + i];
    return count;
}

void WebViewImpl::selection(int& anchor, int& focus) const
{
    anchor = focus = 0;
    Element* root = editableRoot();
    auto& selection = focusedFrame().selection().selection();
    auto range = selection.firstRange();
    if (!root || !range)
        return;
    auto characters = characterRange(makeRangeSelectingNodeContents(*root), *range);
    int start = characters.location;
    int end = characters.location + characters.length;
    anchor = selection.isBaseFirst() ? start : end;
    focus = selection.isBaseFirst() ? end : start;
}

void WebViewImpl::setSelection(int anchor, int focus)
{
    Element* root = editableRoot();
    if (!root)
        return;
    int start = std::max(0, std::min(anchor, focus));
    int length = std::abs(focus - anchor);
    auto range = resolveCharacterRange(makeRangeSelectingNodeContents(*root), CharacterRange(start, length));
    focusedFrame().selection().setSelection(VisibleSelection(range));
}

void WebViewImpl::setComposition(Text text, int cursor)
{
    String composition = toString(text);
    unsigned position = std::max(0, std::min<int>(cursor, composition.length()));
    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, composition.length(), CompositionUnderlineColor::TextColor, Color::black, false));
    focusedFrame().editor().setComposition(composition, underlines, { }, { }, position, position);
}

void WebViewImpl::confirmComposition(Text text)
{
    Editor& editor = focusedFrame().editor();
    String characters = toString(text);
    if (editor.hasComposition())
        editor.confirmComposition(characters);
    else if (!characters.isEmpty())
        editor.insertText(characters, nullptr);
}

void WebViewImpl::cancelComposition()
{
    // Editor::cancelComposition() leaves the marked text in the document; an
    // input method that cancels expects it gone.
    Editor& editor = focusedFrame().editor();
    if (editor.hasComposition())
        editor.confirmComposition(emptyString());
}

Rect WebViewImpl::caretRect() const
{
    LocalFrame& frame = focusedFrame();
    IntRect caret = frame.selection().absoluteCaretBounds();
    if (LocalFrameView* view = frame.view())
        caret = view->contentsToRootView(caret);
    return { caret.x(), caret.y(), caret.width(), caret.height() };
}

} // namespace WebKitSymbian
