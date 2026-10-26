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

#include <WebCore/FrameLoaderClient.h>

namespace WebKitSymbian {

class WebViewImpl;

class SymbianFrameLoaderClient final : public WebCore::FrameLoaderClient {
public:
    explicit SymbianFrameLoaderClient(WebViewImpl&);

    // The frame is created with its client, so it is set afterwards.
    void setFrame(WebCore::Frame& frame) { m_frame = &frame; }

private:
    bool isMainFrame() const;

    Ref<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&) final;

    Optional<WebCore::FrameIdentifier> frameID() const final { return WTF::nullopt; }
    Optional<WebCore::PageIdentifier> pageID() const final { return WTF::nullopt; }

    bool hasWebView() const final { return true; }

    void makeRepresentation(WebCore::DocumentLoader*) final { }
    void forceLayoutForNonHTML() final { }
    void setCopiesOnScroll() final { }
    void detachedFromParent2() final { }
    void detachedFromParent3() final { }

    void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) final { }

    void assignIdentifierToInitialRequest(unsigned long, WebCore::DocumentLoader*, const WebCore::ResourceRequest&) final { }
    bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long) final { return true; }
    void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long, WebCore::ResourceRequest&, const WebCore::ResourceResponse&) final { }
    void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long, const WebCore::AuthenticationChallenge&) final;
    void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long, const WebCore::ResourceResponse&) final { }
    void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long, int) final { }
    void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long) final { }
    void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long, const WebCore::ResourceError&) final { }
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
    void dispatchDidCommitLoad(Optional<WebCore::HasInsecureContent>, Optional<WebCore::UsedLegacyTLS>) final;
    void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&, WebCore::WillContinueLoading) final;
    void dispatchDidFailLoad(const WebCore::ResourceError&) final;
    void dispatchDidFinishDocumentLoad() final { }
    void dispatchDidFinishLoad() final;
    void dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone>) final { }
    void dispatchDidReachVisuallyNonEmptyState() final { }

    WebCore::Frame* dispatchCreatePage(const WebCore::NavigationAction&, WebCore::NewFrameOpenerPolicy) final { return nullptr; }
    void dispatchShow() final { }

    void dispatchDecidePolicyForResponse(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, WebCore::PolicyCheckIdentifier, const String&, WebCore::FramePolicyFunction&&) final;
    void dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, WebCore::FormState*, const String&, WebCore::PolicyCheckIdentifier, WebCore::FramePolicyFunction&&) final;
    void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, WebCore::FormState*, WebCore::PolicyDecisionMode, WebCore::PolicyCheckIdentifier, WebCore::FramePolicyFunction&&) final;
    void cancelPolicyCheck() final { }

    void dispatchUnableToImplementPolicy(const WebCore::ResourceError&) final { }

    void dispatchWillSendSubmitEvent(Ref<WebCore::FormState>&&) final { }
    void dispatchWillSubmitForm(WebCore::FormState&, CompletionHandler<void()>&&) final;

    void revertToProvisionalState(WebCore::DocumentLoader*) final { }
    void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&) final { }
    void setMainFrameDocumentReady(bool) final { }

    void startDownload(const WebCore::ResourceRequest&, const String&) final { }

    void willChangeTitle(WebCore::DocumentLoader*) final { }
    void didChangeTitle(WebCore::DocumentLoader*) final { }
    void willReplaceMultipartContent() final { }
    void didReplaceMultipartContent() final { }

    void committedLoad(WebCore::DocumentLoader*, const char*, int) final;
    void finishedLoading(WebCore::DocumentLoader*) final { }

    WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&) const final;
    WebCore::ResourceError blockedError(const WebCore::ResourceRequest&) const final;
    WebCore::ResourceError blockedByContentBlockerError(const WebCore::ResourceRequest&) const final;
    WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&) const final;
    WebCore::ResourceError interruptedForPolicyChangeError(const WebCore::ResourceRequest&) const final;
    WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&) const final;
    WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&) const final;
    WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&) const final;

    bool shouldFallBack(const WebCore::ResourceError&) const final;

    bool canHandleRequest(const WebCore::ResourceRequest&) const final { return true; }
    bool canShowMIMEType(const String&) const final;
    bool canShowMIMETypeAsHTML(const String&) const final { return false; }
    bool representationExistsForURLScheme(const String&) const final { return false; }
    String generatedMIMETypeForURLScheme(const String&) const final { return { }; }

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
    void transitionToCommittedForNewPage() final;

    void didRestoreFromBackForwardCache() final { }

    void updateGlobalHistory() final { }
    void updateGlobalHistoryRedirectLinks() final { }
    bool shouldGoToHistoryItem(WebCore::HistoryItem&) const final { return true; }
    void saveViewStateToItem(WebCore::HistoryItem&) final { }
    bool canCachePage() const final { return true; }
    void didDisplayInsecureContent() final { }
    void didRunInsecureContent(WebCore::SecurityOrigin&, const URL&) final { }
    void didDetectXSS(const URL&, bool) final { }
    RefPtr<WebCore::Frame> createFrame(const String& name, WebCore::HTMLFrameOwnerElement&) final;
    RefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement&, const URL&, const Vector<String>&, const Vector<String>&, const String&, bool) final { return nullptr; }

    WebCore::ObjectContentType objectContentType(const URL&, const String& mimeType) final;
    String overrideMediaType() const final { return { }; }

    void redirectDataToPlugin(WebCore::Widget&) final { }
    void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) final { }

    Ref<WebCore::FrameNetworkingContext> createNetworkingContext() final;

    void prefetchDNS(const String&) final { }
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>&&)>&&) final;

    WebViewImpl& m_view;
    WebCore::Frame* m_frame { nullptr };
};

} // namespace WebKitSymbian
