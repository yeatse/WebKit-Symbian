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
#include "NetworkStorageSessionMap.h"
#include "PageStorageSessionProvider.h"
#include "SymbianChromeClient.h"
#include "SymbianEditorClient.h"
#include "SymbianFrameLoaderClient.h"
#include "SymbianPlatformStrategies.h"
#include "WebDatabaseProvider.h"
#include "WebStorageNamespaceProvider.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/BackForwardCache.h>
#include <WebCore/BackForwardController.h>
#include <WebCore/CacheStorageProvider.h>
#include <WebCore/CompositionHighlight.h>
#include <WebCore/CompositionUnderline.h>
#include <WebCore/CookieJar.h>
#include <WebCore/CookieJarDB.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DummySpeechRecognitionProvider.h>
#include <WebCore/Editor.h>
#include <WebCore/EventNames.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/LibWebRTCProvider.h>
#include <WebCore/MediaRecorderProvider.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/Page.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/PlatformContextCairo.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/PluginInfoProvider.h>
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
#include <WebCore/UserContentController.h>
#include <WebCore/VisitedLinkStore.h>
#include <WebCore/WindowsKeyboardCodes.h>
#include <cairo.h>
#include <wtf/FileSystem.h>
#include <wtf/HashSet.h>
#include <wtf/HexNumber.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
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
    if (!text.characters)
        return emptyString();
    return String(reinterpret_cast<const UChar*>(text.characters), text.length);
}

class SymbianProgressTrackerClient final : public ProgressTrackerClient {
public:
    explicit SymbianProgressTrackerClient(WebViewImpl& view)
        : m_view(view)
    {
    }

private:
    void progressStarted(Frame&) final { m_view.progressChanged(); }
    void progressEstimateChanged(Frame&) final { m_view.progressChanged(); }
    void progressFinished(Frame&) final { m_view.progressChanged(); }

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

// No plug-ins.
class SymbianPluginInfoProvider final : public PluginInfoProvider {
public:
    static Ref<SymbianPluginInfoProvider> create() { return adoptRef(*new SymbianPluginInfoProvider); }

private:
    void refreshPlugins() final { }
    Vector<PluginInfo> pluginInfo(Page&, Optional<Vector<SupportedPluginIdentifier>>&) final { return { }; }
    Vector<PluginInfo> webVisiblePluginInfo(Page&, const URL&) final { return { }; }
};

ApplicationCacheStorage& applicationCacheStorage()
{
    static NeverDestroyed<Ref<ApplicationCacheStorage>> storage(ApplicationCacheStorage::create(
        FileSystem::pathByAppendingComponent(dataDirectory(), "ApplicationCache"),
        FileSystem::pathByAppendingComponent(dataDirectory(), "ApplicationCache/Flat")));
    return storage.get();
}

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
    return makeString("U+", hex(toASCIIUpper(character), 4));
}

} // namespace

} // namespace WebKitSymbian

String WebDatabaseProvider::indexedDatabaseDirectoryPath()
{
    return FileSystem::pathByAppendingComponent(WebKitSymbian::dataDirectory(), "IndexedDB");
}

namespace WebKitSymbian {

UTF16Text::UTF16Text(const String& string)
{
    m_characters.reserveInitialCapacity(string.length());
    for (unsigned i = 0; i < string.length(); ++i)
        m_characters.uncheckedAppend(string[i]);
}

Text UTF16Text::text() const
{
    return Text { reinterpret_cast<const uint16_t*>(m_characters.data()), static_cast<int>(m_characters.size()) };
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
    NetworkStorageSessionMap::defaultStorageSession().setCookieDatabase(
        makeUniqueRef<CookieJarDB>(FileSystem::pathByAppendingComponent(dataDirectory(), "cookies.db")));

    // A phone's worth of caches.
    MemoryCache::singleton().setCapacities(0, 4 * MB, 8 * MB);
    BackForwardCache::singleton().setMaxSize(1);
}

void WebView::setWakeUpCallback(void (*wakeUp)(void*), void* context)
{
    if (!wakeUp) {
        RunLoop::setWakeUpCallback({ });
        return;
    }
    RunLoop::setWakeUpCallback([wakeUp, context] {
        wakeUp(context);
    });
}

void WebView::runPendingWork()
{
    RunLoop::iterate();
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
    , m_size(width, height)
{
    auto storageProvider = PageStorageSessionProvider::create();
    auto frameLoaderClient = makeUniqueRef<SymbianFrameLoaderClient>(*this);
    auto& mainFrameClient = frameLoaderClient.get();
    PageConfiguration configuration(
        PAL::SessionID::defaultSessionID(),
        makeUniqueRef<SymbianEditorClient>(*this),
        SocketProvider::create(),
        makeUniqueRef<LibWebRTCProvider>(),
        CacheStorageProvider::create(),
        UserContentController::create(),
        BackForwardList::create(),
        CookieJar::create(storageProvider.copyRef()),
        makeUniqueRef<SymbianProgressTrackerClient>(*this),
        WTFMove(frameLoaderClient),
        makeUniqueRef<DummySpeechRecognitionProvider>(),
        makeUniqueRef<MediaRecorderProvider>());
    configuration.chromeClient = new SymbianChromeClient(*this);
    configuration.databaseProvider = &WebDatabaseProvider::singleton();
    configuration.storageNamespaceProvider = WebKit::WebStorageNamespaceProvider::create(
        FileSystem::pathByAppendingComponent(dataDirectory(), "LocalStorage"));
    configuration.visitedLinkStore = SymbianVisitedLinkStore::create();
    configuration.applicationCacheStorage = &applicationCacheStorage();
    configuration.pluginInfoProvider = SymbianPluginInfoProvider::create();

    m_page = makeUnique<Page>(WTFMove(configuration));
    storageProvider->setPage(*m_page);
    mainFrameClient.setFrame(m_page->mainFrame());

    Settings& settings = m_page->settings();
    settings.setScriptEnabled(true);
    settings.setLoadsImagesAutomatically(true);
    settings.setTextAreasAreResizable(false);
    settings.setDefaultTextEncodingName("UTF-8");

    m_page->mainFrame().init();
    m_page->setIsVisible(true);
    m_page->focusController().setActive(true);
}

WebViewImpl::~WebViewImpl()
{
    m_page->mainFrame().loader().detachFromParent();
}

Frame& WebViewImpl::focusedFrame() const
{
    return m_page->focusController().focusedOrMainFrame();
}

void WebViewImpl::invalidate(const IntRect& rect)
{
    IntRect visible = intersection(rect, IntRect(IntPoint(), m_size));
    if (visible.isEmpty())
        return;
    m_client.didInvalidate({ visible.x(), visible.y(), visible.width(), visible.height() });
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
    DocumentLoader* loader = m_page->mainFrame().loader().documentLoader();
    if (!loader)
        return;
    URL url = loader->unreachableURL().isEmpty() ? loader->url() : loader->unreachableURL();
    m_client.didChangeURL(UTF16Text(url.string()).text());
}

void WebViewImpl::progressChanged()
{
    m_client.didChangeProgress(m_page->mainFrame().loader().isLoading(), m_page->progress().estimatedProgress());
}

void WebViewImpl::loadErrorPage(const ResourceError& error)
{
    StringBuilder html;
    html.appendLiteral("<!DOCTYPE html><meta name=viewport content='width=device-width'>"
        "<title>Problem loading page</title><body style='font-family:sans-serif;margin:16px'>"
        "<h3>This page could not be loaded</h3><p>");
    html.append(error.localizedDescription().isEmpty() ? "Unknown error"_s : error.localizedDescription());
    html.appendLiteral("</p><p style='word-wrap:break-word;color:#666'>");
    html.append(error.failingURL().string());
    html.appendLiteral("</p></body>");
    CString utf8 = html.toString().utf8();
    URL failingURL = error.failingURL();
    ResourceResponse response(failingURL, "text/html", utf8.length(), "UTF-8");
    SubstituteData substituteData(SharedBuffer::create(utf8.data(), utf8.length()), failingURL, response, SubstituteData::SessionHistoryVisibility::Hidden);
    Frame& frame = m_page->mainFrame();
    frame.loader().load(FrameLoadRequest(frame, ResourceRequest(failingURL), substituteData));
}

void WebViewImpl::setSize(int width, int height)
{
    m_size = IntSize(width, height);
    if (FrameView* view = m_page->mainFrame().view())
        view->resize(m_size);
    invalidate(IntRect(IntPoint(), m_size));
}

void WebViewImpl::paint(uint8_t* pixels, int stride, const Rect& area)
{
    IntRect rect = intersection(IntRect(area.x, area.y, area.width, area.height), IntRect(IntPoint(), m_size));
    if (rect.isEmpty())
        return;
    cairo_surface_t* surface = cairo_image_surface_create_for_data(pixels, CAIRO_FORMAT_RGB24, m_size.width(), m_size.height(), stride);
    cairo_t* cr = cairo_create(surface);
    {
        PlatformContextCairo platformContext(cr);
        GraphicsContext context(&platformContext);
        context.clip(rect);
        FrameView* view = m_page->mainFrame().view();
        if (view) {
            view->updateLayoutAndStyleIfNeededRecursive();
            view->paint(context, rect);
        } else
            context.fillRect(rect, Color::white);
    }
    cairo_destroy(cr);
    cairo_surface_flush(surface);
    cairo_surface_destroy(surface);
}

void WebViewImpl::load(const char* url)
{
    String string = String::fromUTF8(url).stripWhiteSpace();
    if (string.isEmpty())
        return;
    // Phone paths (C:\Data\page.html) are files, not URLs with a "c" scheme.
    if (string.length() > 2 && isASCIIAlpha(string[0]) && string[1] == ':' && (string[2] == '/' || string[2] == '\\'))
        string = makeString("file:///", string.replace('\\', '/'));
    URL parsed(URL(), string);
    if (!parsed.isValid() || parsed.protocol().isEmpty() || !string.contains(':'))
        parsed = URL(URL(), makeString("http://", string));
    Frame& frame = m_page->mainFrame();
    frame.loader().load(FrameLoadRequest(frame, ResourceRequest(parsed)));
}

void WebViewImpl::reload()
{
    m_page->mainFrame().loader().reload();
}

void WebViewImpl::stop()
{
    m_page->mainFrame().loader().stopAllLoaders();
}

void WebViewImpl::goBack()
{
    m_page->backForward().goBack();
}

void WebViewImpl::goForward()
{
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

void WebViewImpl::setFocused(bool focused)
{
    m_page->focusController().setActive(true);
    m_page->focusController().setFocused(focused);
}

static const int tapSlop = 8;

void WebViewImpl::handlePointer(PointerEvent event, int x, int y)
{
    IntPoint point(x, y);
    switch (event) {
    case PointerEvent::Down:
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
            click(m_pointerStart);
        m_pointerDown = false;
        break;
    }
}

void WebViewImpl::click(const IntPoint& point)
{
    auto& handler = m_page->mainFrame().eventHandler();
    WallTime now = WallTime::now();
    handler.mouseMoved(PlatformMouseEvent(point, point, NoButton, PlatformEvent::MouseMoved, 0, false, false, false, false, now, 0, NoTap));
    handler.handleMousePressEvent(PlatformMouseEvent(point, point, LeftButton, PlatformEvent::MousePressed, 1, false, false, false, false, now, ForceAtClick, OneFingerTap));
    handler.handleMouseReleaseEvent(PlatformMouseEvent(point, point, LeftButton, PlatformEvent::MouseReleased, 1, false, false, false, false, now, ForceAtClick, OneFingerTap));
}

void WebViewImpl::scrollAt(const IntPoint& point, const IntSize& delta)
{
    // A wheel event scrolls whatever is under the finger, overflow areas
    // included; the content follows the finger.
    PlatformWheelEvent event(point, point, delta.width(), delta.height(), 0, 0, ScrollByPixelWheelEvent, false, false, false, false);
    m_page->mainFrame().eventHandler().handleWheelEvent(event, { WheelEventProcessingSteps::MainThreadForScrolling, WheelEventProcessingSteps::MainThreadForBlockingDOMEventDispatch });
}

void WebViewImpl::scrollBy(int dx, int dy)
{
    if (FrameView* view = m_page->mainFrame().view())
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
    String characters = toString(text);
    String key = keyForKeyCode(keyCode, characters);
    String keyIdentifier = keyIdentifierForKeyCode(keyCode, characters);
    WallTime now = WallTime::now();
    auto& handler = focusedFrame().eventHandler();
    if (!isDown)
        return handler.keyEvent(PlatformKeyboardEvent(PlatformEvent::KeyUp, { }, { }, key, { }, keyIdentifier, keyCode, false, false, false, eventModifiers, now));

    // As on Windows: a raw key down, then the characters it typed.
    bool handled = handler.keyEvent(PlatformKeyboardEvent(PlatformEvent::RawKeyDown, { }, { }, key, { }, keyIdentifier, keyCode, false, false, false, eventModifiers, now));
    if (handled || characters.isEmpty())
        return handled;
    return focusedFrame().eventHandler().keyEvent(PlatformKeyboardEvent(PlatformEvent::Char, characters, characters, key, { }, keyIdentifier, keyCode, false, false, false, eventModifiers, now));
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

    auto command = frame->editor().command(interpretKeyEvent(event));
    if (keyEvent->type() == PlatformEvent::RawKeyDown) {
        // Commands that insert text wait for the key press, so the page sees it.
        return !command.isTextInsertion() && command.execute(&event);
    }
    if (command.execute(&event))
        return true;
    if (event.charCode() < ' ')
        return false;
    return frame->editor().insertText(keyEvent->text(), &event);
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
    focusedFrame().editor().setComposition(composition, underlines, { }, position, position);
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
    focusedFrame().editor().cancelComposition();
}

Rect WebViewImpl::caretRect() const
{
    Frame& frame = focusedFrame();
    IntRect caret = frame.selection().absoluteCaretBounds();
    if (FrameView* view = frame.view())
        caret = view->contentsToRootView(caret);
    return { caret.x(), caret.y(), caret.width(), caret.height() };
}

} // namespace WebKitSymbian
