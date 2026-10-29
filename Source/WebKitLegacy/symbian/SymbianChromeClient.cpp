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

#include "SymbianChromeClient.h"

#include "WebViewImpl.h"
#include <WebCore/LocalFrameView.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ColorChooser.h>
#include <WebCore/CookieConsentDecisionResult.h>
#include <WebCore/DataListSuggestionPicker.h>
#include <WebCore/DateTimeChooser.h>
#include <WebCore/FocusOptions.h>
#include <WebCore/Page.h>
#include <WebCore/PopupMenu.h>
#include <WebCore/PopupMenuClient.h>
#include <WebCore/SearchPopupMenu.h>

namespace WebKitSymbian {

using namespace WebCore;

namespace {

// A <select> list shown by the application, which blocks until the user
// picks an item.
class SymbianPopupMenu final : public PopupMenu {
public:
    SymbianPopupMenu(PopupMenuClient& client, WebViewImpl& view)
        : m_client(&client)
        , m_view(view)
    {
    }

private:
    void show(const IntRect&, LocalFrameView&, int selectedIndex) final
    {
        if (!m_client)
            return;
        Ref<SymbianPopupMenu> protectedThis(*this);
        int count = m_client->listSize();
        Vector<String> strings;
        Vector<int> indices;
        for (int i = 0; i < count; ++i) {
            if (m_client->itemIsSeparator(i) || m_client->itemIsLabel(i) || !m_client->itemIsEnabled(i))
                continue;
            strings.append(m_client->itemText(i));
            indices.append(i);
        }
        Vector<UTF16Text> texts;
        Vector<Text> items;
        int selected = -1;
        for (size_t i = 0; i < strings.size(); ++i) {
            texts.append(UTF16Text(strings[i]));
            if (indices[i] == selectedIndex)
                selected = i;
        }
        for (auto& text : texts)
            items.append(text.text());
        int chosen = m_view.client().runSelectPopup(items.span().data(), items.size(), selected);
        if (!m_client)
            return;
        if (chosen >= 0 && static_cast<size_t>(chosen) < indices.size())
            m_client->valueChanged(indices[chosen]);
        if (m_client)
            m_client->popupDidHide();
    }

    void hide() final { }
    void updateFromElement() final { }
    void disconnectClient() final { m_client = nullptr; }

    PopupMenuClient* m_client;
    WebViewImpl& m_view;
};

class SymbianSearchPopupMenu final : public SearchPopupMenu {
public:
    SymbianSearchPopupMenu(PopupMenuClient& client, WebViewImpl& view)
        : m_popup(adoptRef(*new SymbianPopupMenu(client, view)))
    {
    }

private:
    PopupMenu* popupMenu() final { return m_popup.ptr(); }
    void saveRecentSearches(const AtomString&, const Vector<RecentSearch>&) final { }
    void loadRecentSearches(const AtomString&, Vector<RecentSearch>&) final { }
    bool enabled() final { return false; }

    Ref<SymbianPopupMenu> m_popup;
};

} // namespace

SymbianChromeClient::SymbianChromeClient(WebViewImpl& view)
    : m_view(view)
{
}

void SymbianChromeClient::chromeDestroyed()
{

}

FloatRect SymbianChromeClient::windowRect() const
{
    return FloatRect(FloatPoint(), m_view.size());
}

FloatRect SymbianChromeClient::pageRect() const
{
    return windowRect();
}

void SymbianChromeClient::focusedElementChanged(Element*, LocalFrame*, FocusOptions, BroadcastFocusedElement)
{
    m_view.focusedElementChanged();
}

void SymbianChromeClient::addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const String& message, unsigned lineNumber, unsigned, const String& sourceID)
{
    WTFLogAlways("console: %s:%u: %s", sourceID.utf8().data(), lineNumber, message.utf8().data());
}

bool SymbianChromeClient::runBeforeUnloadConfirmPanel(String&& message, LocalFrame&)
{
    return m_view.client().runJavaScriptConfirm(UTF16Text(message).text());
}

void SymbianChromeClient::runJavaScriptAlert(LocalFrame&, const String& message)
{
    m_view.client().runJavaScriptAlert(UTF16Text(message).text());
}

bool SymbianChromeClient::runJavaScriptConfirm(LocalFrame&, const String& message)
{
    return m_view.client().runJavaScriptConfirm(UTF16Text(message).text());
}

RefPtr<PopupMenu> SymbianChromeClient::createPopupMenu(PopupMenuClient& client) const
{
    return adoptRef(*new SymbianPopupMenu(client, m_view));
}

RefPtr<SearchPopupMenu> SymbianChromeClient::createSearchPopupMenu(PopupMenuClient& client) const
{
    return adoptRef(*new SymbianSearchPopupMenu(client, m_view));
}

void SymbianChromeClient::invalidateContentsAndRootView(const IntRect& rect)
{
    m_view.invalidate(rect);
}

void SymbianChromeClient::invalidateContentsForSlowScroll(const IntRect& rect)
{
    m_view.invalidate(rect);
}

void SymbianChromeClient::scroll(const IntSize&, const IntRect& scrollViewRect, const IntRect&)
{
    // No blitting: the whole scrolled area is painted again.
    m_view.invalidate(scrollViewRect);
}

void SymbianChromeClient::showShareSheet(ShareDataWithParsedURL&&, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(false);
}

void SymbianChromeClient::dispatchViewportPropertiesDidChange(const ViewportArguments& arguments) const
{
    m_view.viewportChanged(arguments);
}

void SymbianChromeClient::triggerRenderingUpdate()
{
    // No compositing: an update runs animation frames and layout, and the
    // repaints it causes arrive as invalidations.
    m_view.page().isolatedUpdateRendering();
}

RefPtr<ColorChooser> SymbianChromeClient::createColorChooser(ColorChooserClient&, const Color&) { return nullptr; }
RefPtr<DataListSuggestionPicker> SymbianChromeClient::createDataListSuggestionPicker(DataListSuggestionsClient&) { return nullptr; }
RefPtr<DateTimeChooser> SymbianChromeClient::createDateTimeChooser(DateTimeChooserClient&) { return nullptr; }
void SymbianChromeClient::requestCookieConsent(CompletionHandler<void(CookieConsentDecisionResult)>&& completion)
{
    completion(CookieConsentDecisionResult::NotSupported);
}

} // namespace WebKitSymbian
