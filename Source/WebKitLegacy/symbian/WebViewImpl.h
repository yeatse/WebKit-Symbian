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

#pragma once

#include "WebKitSymbian.h"
#include <WebCore/IntPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/Timer.h>
#include <WebCore/ViewportArguments.h>
#include <memory>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class Element;
class Frame;
class KeyboardEvent;
class Page;
class ResourceError;
}

namespace WebKitSymbian {

// UTF-16 storage for a Text handed to the application.
class UTF16Text {
public:
    explicit UTF16Text(const String&);
    Text text() const;

private:
    Vector<UChar> m_characters;
};

class WebViewImpl final : public WebView {
public:
    WebViewImpl(WebViewClient&, int width, int height);
    ~WebViewImpl();

    WebViewClient& client() const { return m_client; }
    WebCore::Page& page() const { return *m_page; }
    WebCore::IntSize size() const { return m_size; }

    // From the WebCore clients.
    void invalidate(const WebCore::IntRect&);
    void focusedElementChanged();
    void selectionChanged();
    bool handleEditingKeyboardEvent(WebCore::KeyboardEvent&);
    void titleChanged(const String&);
    void urlChanged();
    void progressChanged();
    void loadErrorPage(const WebCore::ResourceError&);
    void viewportChanged(const WebCore::ViewportArguments&);

private:
    // WebView
    void setSize(int width, int height) final;
    void paint(uint8_t* pixels, int stride, const Rect&) final;
    void load(const char* url) final;
    void reload() final;
    void stop() final;
    void goBack() final;
    void goForward() final;
    bool canGoBack() const final;
    bool canGoForward() const final;
    void setFocused(bool) final;
    void handlePointer(PointerEvent, int x, int y) final;
    void scrollBy(int dx, int dy) final;
    bool handleKey(bool isDown, int windowsKeyCode, Text, unsigned modifiers) final;
    InputType focusedInputType() const final;
    int editableTextLength() const final;
    int maximumEditableTextLength() const final;
    int copyEditableText(int start, int length, uint16_t* buffer) const final;
    void selection(int& anchor, int& focus) const final;
    void setSelection(int anchor, int focus) final;
    void setComposition(Text, int cursor) final;
    void confirmComposition(Text) final;
    void cancelComposition() final;
    Rect caretRect() const final;

    WebCore::Frame& focusedFrame() const;
    WebCore::Element* editableRoot() const;
    WebCore::ViewportAttributes viewportAttributes() const;
    void updateLayoutSize();
    void updateScale();
    void tap(const WebCore::IntPoint&);
    void zoomAt(const WebCore::IntPoint&);
    void click(const WebCore::IntPoint&);
    void scrollAt(const WebCore::IntPoint&, const WebCore::IntSize& delta);

    WebViewClient& m_client;
    std::unique_ptr<WebCore::Page> m_page;
    WebCore::IntSize m_size;

    // Pages lay out at the width their viewport asks for, 980 pixels for
    // those without one, and start scaled to fit the view.
    WebCore::ViewportArguments m_viewportArguments;
    WebCore::Timer m_scaleTimer;
    float m_initialScale { 1 };
    float m_minimumScale { 1 };
    float m_maximumScale { 1 };

    // Touch: a press that moves further than the tap slop scrolls.
    bool m_pointerDown { false };
    bool m_pointerScrolling { false };
    WebCore::IntPoint m_pointerStart;
    WebCore::IntPoint m_pointerLast;
    // A second tap close to the first zooms.
    MonotonicTime m_lastTapTime;
    WebCore::IntPoint m_lastTap;
};

} // namespace WebKitSymbian
