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

#include <WebCore/ChromeClient.h>

namespace WebKitSymbian {

class WebViewImpl;

class SymbianChromeClient final : public WebCore::ChromeClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SymbianChromeClient(WebViewImpl&);

private:
    void chromeDestroyed() final;

    void setWindowRect(const WebCore::FloatRect&) final { }
    WebCore::FloatRect windowRect() final;
    WebCore::FloatRect pageRect() final;

    void focus() final { }
    void unfocus() final { }
    bool canTakeFocus(WebCore::FocusDirection) final { return true; }
    void takeFocus(WebCore::FocusDirection) final { }

    void focusedElementChanged(WebCore::Element*) final;
    void focusedFrameChanged(WebCore::Frame*) final { }

    // Pages open in the same view.
    WebCore::Page* createWindow(WebCore::Frame&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&) final { return nullptr; }
    void show() final { }

    bool canRunModal() final { return false; }
    void runModal() final { }

    void setToolbarsVisible(bool) final { }
    bool toolbarsVisible() final { return false; }
    void setStatusbarVisible(bool) final { }
    bool statusbarVisible() final { return false; }
    void setScrollbarsVisible(bool) final { }
    bool scrollbarsVisible() final { return true; }
    void setMenubarVisible(bool) final { }
    bool menubarVisible() final { return false; }
    void setResizable(bool) final { }

    void addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID) final;

    bool canRunBeforeUnloadConfirmPanel() final { return true; }
    bool runBeforeUnloadConfirmPanel(const String& message, WebCore::Frame&) final;

    void closeWindowSoon() final { }

    void runJavaScriptAlert(WebCore::Frame&, const String&) final;
    bool runJavaScriptConfirm(WebCore::Frame&, const String&) final;
    bool runJavaScriptPrompt(WebCore::Frame&, const String&, const String&, String&) final { return false; }

    bool selectItemWritingDirectionIsNatural() final { return false; }
    bool selectItemAlignmentFollowsMenuWritingDirection() final { return true; }
    RefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient&) const final;
    RefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient&) const final;

    void setStatusbarText(const String&) final { }

    WebCore::KeyboardUIMode keyboardUIMode() final { return WebCore::KeyboardAccessDefault; }

    // A touch screen: no hover, a coarse pointer.
    bool hoverSupportedByPrimaryPointingDevice() const final { return false; }
    bool hoverSupportedByAnyAvailablePointingDevice() const final { return false; }
    Optional<WebCore::PointerCharacteristics> pointerCharacteristicsOfPrimaryPointingDevice() const final { return WebCore::PointerCharacteristics::Coarse; }
    OptionSet<WebCore::PointerCharacteristics> pointerCharacteristicsOfAllAvailablePointingDevices() const final { return WebCore::PointerCharacteristics::Coarse; }

    void invalidateRootView(const WebCore::IntRect&) final { }
    void invalidateContentsAndRootView(const WebCore::IntRect&) final;
    void invalidateContentsForSlowScroll(const WebCore::IntRect&) final;
    void scroll(const WebCore::IntSize&, const WebCore::IntRect&, const WebCore::IntRect&) final;

    WebCore::IntPoint screenToRootView(const WebCore::IntPoint& point) const final { return point; }
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect& rect) const final { return rect; }
    WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint& point) const final { return point; }
    WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect& rect) const final { return rect; }

    void didFinishLoadingImageForElement(WebCore::HTMLImageElement&) final { }

    PlatformPageClient platformPageClient() const final { return nullptr; }
    void contentsSizeChanged(WebCore::Frame&, const WebCore::IntSize&) const final { }
    void intrinsicContentsSizeChanged(const WebCore::IntSize&) const final { }

    void mouseDidMoveOverElement(const WebCore::HitTestResult&, unsigned, const String&, WebCore::TextDirection) final { }

    void print(WebCore::Frame&, const WebCore::StringWithDirection&) final { }

    void exceededDatabaseQuota(WebCore::Frame&, const String&, WebCore::DatabaseDetails) final { }
    void reachedMaxAppCacheSize(int64_t) final { }
    void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin&, int64_t) final { }

    void runOpenPanel(WebCore::Frame&, WebCore::FileChooser&) final { }
    void showShareSheet(WebCore::ShareDataWithParsedURL&, CompletionHandler<void(bool)>&&) final;
    void loadIconForFiles(const Vector<String>&, WebCore::FileIconLoader&) final { }

    void elementDidFocus(WebCore::Element&) final { }
    void elementDidBlur(WebCore::Element&) final { }

    void setCursor(const WebCore::Cursor&) final { }
    void setCursorHiddenUntilMouseMoves(bool) final { }

    void scrollRectIntoView(const WebCore::IntRect&) const final { }

    void attachRootGraphicsLayer(WebCore::Frame&, WebCore::GraphicsLayer*) final { }
    void attachViewOverlayGraphicsLayer(WebCore::GraphicsLayer*) final { }
    void setNeedsOneShotDrawingSynchronization() final { }
    void triggerRenderingUpdate() final;

#if ENABLE(ORIENTATION_EVENTS)
    int deviceOrientation() const final { return 0; }
#endif

    void wheelEventHandlersChanged(bool) final { }

    void didAssociateFormControls(const Vector<RefPtr<WebCore::Element>>&, WebCore::Frame&) final { }
    bool shouldNotifyOnFormChanges() final { return false; }

    RefPtr<WebCore::Icon> createIconForFiles(const Vector<String>&) final { return nullptr; }

    WebViewImpl& m_view;
};

} // namespace WebKitSymbian
