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

#include <WebCore/LocalFrameLoaderClient.h>
#include <wtf/CompletionHandler.h>

namespace WebKitSymbian {

class WebViewImpl;

class SymbianFrameLoaderClient final : public WebCore::LocalFrameLoaderClient {
public:
    SymbianFrameLoaderClient(WebViewImpl&, WebCore::LocalFrame&, WebCore::FrameLoader&);


private:
    bool isMainFrame() const;

    Ref<WebCore::DocumentLoader> createDocumentLoader(WebCore::ResourceRequest&&, WebCore::SubstituteData&&) final;


    bool hasWebView() const final { return true; }

    void makeRepresentation(WebCore::DocumentLoader*) final { }
    void forceLayoutForNonHTML() final { }
    void setCopiesOnScroll() final { }
    void detachedFromParent2() final { }
    void detachedFromParent3() final { }

    void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) final { }

    void assignIdentifierToInitialRequest(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&) final { }
    bool shouldUseCredentialStorage(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier) final { return true; }
    void dispatchWillSendRequest(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse&) final { }
    void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, const WebCore::AuthenticationChallenge&) final;
    void dispatchDidReceiveResponse(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, const WebCore::ResourceResponse&) final { }
    void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, int) final { }
    void dispatchDidFinishLoading(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier) final { }
    void dispatchDidFailLoading(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, const WebCore::ResourceError&) final { }
    bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int) final { return false; }

    void dispatchDidDispatchOnloadEvents() final { }
    void dispatchDidReceiveServerRedirectForProvisionalLoad() final { }
    void dispatchDidCancelClientRedirect() final { }
    void dispatchWillPerformClientRedirect(const URL&, double, WallTime, WebCore::LockBackForwardList) final { }
    void dispatchDidChangeLocationWithinPage() final;
    void dispatchDidPushStateWithinPage() final;
    void dispatchDidReplaceStateWithinPage() final;
    void dispatchDidPopStateWithinPage() final;
    void dispatchWillClose() final { }
    void dispatchDidStartProvisionalLoad() final;
    void dispatchDidReceiveTitle(const WebCore::StringWithDirection&) final;
    void dispatchDidCommitLoad(std::optional<WebCore::HasInsecureContent>, std::optional<WebCore::UsedLegacyTLS>, std::optional<WebCore::WasPrivateRelayed>) final;
    void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&, WebCore::WillContinueLoading, WebCore::WillInternallyHandleFailure) final;
    void dispatchDidFailLoad(const WebCore::ResourceError&) final;
    void dispatchDidFinishDocumentLoad() final { }
    void dispatchDidFinishLoad() final;
    void dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone>) final { }
    void dispatchDidReachVisuallyNonEmptyState() final { }

    WebCore::LocalFrame* dispatchCreatePage(const WebCore::NavigationAction&, WebCore::NewFrameOpenerPolicy) final { return nullptr; }
    void dispatchShow() final { }

    void dispatchDecidePolicyForResponse(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, const String&, WebCore::FramePolicyFunction&&) final;
    void dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, WebCore::FormState*, const String&, std::optional<WebCore::HitTestResult>&&, WebCore::FramePolicyFunction&&) final;
    void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, WebCore::FormState*, const String&, std::optional<WebCore::NavigationIdentifier>, std::optional<WebCore::HitTestResult>&&, bool, WebCore::NavigationUpgradeToHTTPSBehavior, WebCore::SandboxFlags, WebCore::PolicyDecisionMode, WebCore::FramePolicyFunction&&) final;
    void cancelPolicyCheck() final { }

    void dispatchUnableToImplementPolicy(const WebCore::ResourceError&) final { }

    void dispatchWillSendSubmitEvent(Ref<WebCore::FormState>&&) final { }
    void dispatchWillSubmitForm(WebCore::FormState&, URL&&, String&&, CompletionHandler<void()>&&) final;

    void revertToProvisionalState(WebCore::DocumentLoader*) final { }
    void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&) final { }
    void setMainFrameDocumentReady(bool) final { }

    void startDownload(const WebCore::ResourceRequest&, const String&, WebCore::FromDownloadAttribute) final { }

    void willChangeTitle(WebCore::DocumentLoader*) final { }
    void didChangeTitle(WebCore::DocumentLoader*) final { }
    void willReplaceMultipartContent() final { }
    void didReplaceMultipartContent() final { }

    void committedLoad(WebCore::DocumentLoader*, const WebCore::SharedBuffer&) final;
    void finishedLoading(WebCore::DocumentLoader*) final { }


    bool shouldFallBack(const WebCore::ResourceError&) const final;

    bool canHandleRequest(const WebCore::ResourceRequest&) const final { return true; }
    bool canShowMIMEType(const String&) const final;
    bool canShowMIMETypeAsHTML(const String&) const final { return false; }
    bool representationExistsForURLScheme(StringView) const final { return false; }
    String generatedMIMETypeForURLScheme(StringView) const final { return { }; }

    void frameLoadCompleted() final { }
    void restoreViewState() final { }
    void provisionalLoadStarted() final { }
    void didFinishLoad() final { }
    void prepareForDataSourceReplacement() final { }

    void updateCachedDocumentLoader(WebCore::DocumentLoader&) final { }
    void setTitle(const WebCore::StringWithDirection&, const URL&) final { }

    String userAgent(const URL&) const final;

    void savePlatformDataToCachedFrame(WebCore::CachedFrame*) final { }
    void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) final { }
    void transitionToCommittedForNewPage(InitializingIframe) final;

    void didRestoreFromBackForwardCache() final { }

    void updateGlobalHistory() final { }
    void updateGlobalHistoryRedirectLinks() final { }
    WebCore::ShouldGoToHistoryItem shouldGoToHistoryItem(WebCore::HistoryItem&, WebCore::IsSameDocumentNavigation, WebCore::ProcessSwapDisposition) const final { return WebCore::ShouldGoToHistoryItem::Yes; }
    void saveViewStateToItem(WebCore::HistoryItem&) final { }
    bool canCachePage() const final { return true; }
    RefPtr<WebCore::LocalFrame> createFrame(const AtomString& name, WebCore::HTMLFrameOwnerElement&) final;
    RefPtr<WebCore::Widget> createPlugin(WebCore::HTMLPlugInElement&, const URL&, const Vector<AtomString>&, const Vector<AtomString>&, const String&, bool) final { return nullptr; }

    WebCore::ObjectContentType objectContentType(const URL&, const String& mimeType) final;
    AtomString overrideMediaType() const final { return { }; }

    void redirectDataToPlugin(WebCore::Widget&) final { }
    void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) final { }

    Ref<WebCore::FrameNetworkingContext> createNetworkingContext() final;

    void prefetchDNS(const String&) final { }
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>&&)>&&) final;

    void updateSandboxFlags(WebCore::SandboxFlags) final { }
    void updateOpener(std::optional<WebCore::FrameIdentifier>) final { }
    void setPrinting(bool, WebCore::FloatSize, WebCore::FloatSize, float, WebCore::AdjustViewSize) final { }
    void loadStorageAccessQuirksIfNeeded() final { }
    bool supportsAsyncShouldGoToHistoryItem() const final { return false; }
    void shouldGoToHistoryItemAsync(WebCore::HistoryItem&, CompletionHandler<void(WebCore::ShouldGoToHistoryItem)>&& completion) const final { completion(WebCore::ShouldGoToHistoryItem::Yes); }
    void dispatchLoadEventToOwnerElementInAnotherProcess() final { }
    RefPtr<WebCore::HistoryItem> createHistoryItemTree(bool, WebCore::BackForwardItemIdentifier) const final;

    WebViewImpl& m_view;
    WebCore::LocalFrame* m_frame { nullptr };
};

} // namespace WebKitSymbian
