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
    WTF_MAKE_TZONE_ALLOCATED_INLINE(SymbianChromeClient);
public:
    explicit SymbianChromeClient(WebViewImpl&);

private:
    void chromeDestroyed() final;

    void setWindowRect(const WebCore::FloatRect&) final { }
    WebCore::FloatRect windowRect() const final;
    WebCore::FloatRect pageRect() const final;

    void focus() final { }
    void unfocus() final { }
    bool canTakeFocus(WebCore::FocusDirection) const final { return true; }
    void takeFocus(WebCore::FocusDirection) final { }

    void focusedElementChanged(WebCore::Element*, WebCore::LocalFrame*, WebCore::FocusOptions, WebCore::BroadcastFocusedElement) final;
    void focusedFrameChanged(WebCore::Frame*) final { }

    // Pages open in the same view.
    RefPtr<WebCore::Page> createWindow(WebCore::LocalFrame&, const String&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&) final { return nullptr; }
    void show() final { }

    bool canRunModal() const final { return false; }
    void runModal() final { }

    bool toolbarsVisible() const final { return false; }
    bool statusbarVisible() const final { return false; }
    bool scrollbarsVisible() const final { return true; }
    bool menubarVisible() const final { return false; }
    void setResizable(bool) final { }

    void addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID) final;

    bool canRunBeforeUnloadConfirmPanel() final { return true; }
    bool runBeforeUnloadConfirmPanel(String&& message, WebCore::LocalFrame&) final;

    void closeWindow() final { }

    void runJavaScriptAlert(WebCore::LocalFrame&, const String&) final;
    bool runJavaScriptConfirm(WebCore::LocalFrame&, const String&) final;
    bool runJavaScriptPrompt(WebCore::LocalFrame&, const String&, const String&, String&) final;

    bool selectItemWritingDirectionIsNatural() final { return false; }
    bool selectItemAlignmentFollowsMenuWritingDirection() final { return true; }
    RefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient&) const final;
    RefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient&) const final;


    WebCore::KeyboardUIMode keyboardUIMode() final { return WebCore::KeyboardAccessDefault; }

    // A touch screen: no hover, a coarse pointer.
    bool hoverSupportedByPrimaryPointingDevice() const final { return false; }
    bool hoverSupportedByAnyAvailablePointingDevice() const final { return false; }
    std::optional<WebCore::PointerCharacteristics> pointerCharacteristicsOfPrimaryPointingDevice() const final { return WebCore::PointerCharacteristics::Coarse; }
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
    void contentsSizeChanged(WebCore::LocalFrame&, const WebCore::IntSize&) const final { }
    void intrinsicContentsSizeChanged(const WebCore::IntSize&) const final { }

    void mouseDidMoveOverElement(const WebCore::HitTestResult&, OptionSet<WebCore::PlatformEventModifier>, const String&, WebCore::TextDirection) final { }

    void print(WebCore::LocalFrame&, const WebCore::StringWithDirection&) final { }

    void exceededDatabaseQuota(WebCore::LocalFrame&, const String&, WebCore::DatabaseDetails) final { }
    void reachedMaxAppCacheSize(int64_t) final { }

    void runOpenPanel(WebCore::LocalFrame&, WebCore::FileChooser&) final;
    void showShareSheet(WebCore::ShareDataWithParsedURL&&, CompletionHandler<void(bool)>&&) final;
    void loadIconForFiles(const Vector<String>&, WebCore::FileIconLoader&) final { }

    void elementDidFocus(WebCore::Element&, const WebCore::FocusOptions&) final { }
    void elementDidBlur(WebCore::Element&) final { }

    void setCursor(const WebCore::Cursor&) final { }
    void setCursorHiddenUntilMouseMoves(bool) final { }

    void scrollContainingScrollViewsToRevealRect(const WebCore::IntRect&) const final { }

    void dispatchViewportPropertiesDidChange(const WebCore::ViewportArguments&) const final;

    bool allowsAcceleratedCompositing() const final;
    void attachRootGraphicsLayer(WebCore::LocalFrame&, WebCore::GraphicsLayer*) final;
    void attachViewOverlayGraphicsLayer(WebCore::GraphicsLayer*) final;
    void setNeedsOneShotDrawingSynchronization() final { }
    void triggerRenderingUpdate() final;

#if ENABLE(ORIENTATION_EVENTS)
    WebCore::IntDegrees deviceOrientation() const final { return 0; }
#endif

    void wheelEventHandlersChanged(bool) final { }

    void didAssociateFormControls(const Vector<Ref<WebCore::Element>>&, WebCore::LocalFrame&) final { }
    bool shouldNotifyOnFormChanges() final { return false; }

    RefPtr<WebCore::Icon> createIconForFiles(const Vector<String>&) final { return nullptr; }

    bool hasAccessoryMousePointingDevice() const final { return false; }
    void rootFrameAdded(const WebCore::LocalFrame&) final { }
    void rootFrameRemoved(const WebCore::LocalFrame&) final { }
    WebCore::IntPoint rootViewToScreen(const WebCore::IntPoint& point) const final { return point; }
    void scrollMainFrameToRevealRect(const WebCore::IntRect&) const final { }

    RefPtr<WebCore::ColorChooser> createColorChooser(WebCore::ColorChooserClient&, const WebCore::Color&) final;
    RefPtr<WebCore::DataListSuggestionPicker> createDataListSuggestionPicker(WebCore::DataListSuggestionsClient&) final;
    bool canShowDataListSuggestionLabels() const final { return false; }
    RefPtr<WebCore::DateTimeChooser> createDateTimeChooser(WebCore::DateTimeChooserClient&) final;
    void setTextIndicator(RefPtr<WebCore::TextIndicator>&&) const final { }
    void updateTextIndicator(RefPtr<WebCore::TextIndicator>&&) const final { }
    void requestCookieConsent(CompletionHandler<void(WebCore::CookieConsentDecisionResult)>&&) final;

    WebViewImpl& m_view;
};

} // namespace WebKitSymbian
