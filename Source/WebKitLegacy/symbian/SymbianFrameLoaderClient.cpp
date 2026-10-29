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

#include "SymbianFrameLoaderClient.h"

#include "NetworkStorageSessionMap.h"
#include "WebViewImpl.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameNetworkingContext.h>
#include <WebCore/FrameTree.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HistoryController.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/FrameTreeSyncData.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/Page.h>
#include <WebCore/PolicyChecker.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/UserAgent.h>

namespace WebKitSymbian {

using namespace WebCore;

namespace {

class SymbianFrameNetworkingContext final : public FrameNetworkingContext {
public:
    static Ref<SymbianFrameNetworkingContext> create(LocalFrame* frame)
    {
        return adoptRef(*new SymbianFrameNetworkingContext(frame));
    }

private:
    explicit SymbianFrameNetworkingContext(LocalFrame* frame)
        : FrameNetworkingContext(frame)
    {
    }

    NetworkStorageSession* storageSession() const final { return &NetworkStorageSessionMap::defaultStorageSession(); }
};

} // namespace

SymbianFrameLoaderClient::SymbianFrameLoaderClient(WebViewImpl& view, LocalFrame& frame, FrameLoader& loader)
    : LocalFrameLoaderClient(loader)
    , m_view(view)
    , m_frame(&frame)
{
}

bool SymbianFrameLoaderClient::isMainFrame() const
{
    return m_frame && m_frame->isMainFrame();
}

Ref<DocumentLoader> SymbianFrameLoaderClient::createDocumentLoader(ResourceRequest&& request, SubstituteData&& substituteData)
{
    return DocumentLoader::create(WTF::move(request), WTF::move(substituteData));
}

void SymbianFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge& challenge)
{
    // FIXME: Ask for credentials.
    if (auto* client = challenge.authenticationClient())
        client->receivedRequestToContinueWithoutCredential(challenge);
}

void SymbianFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    if (isMainFrame())
        m_view.urlChanged();
}

void SymbianFrameLoaderClient::dispatchDidPushStateWithinPage()
{
    dispatchDidChangeLocationWithinPage();
}

void SymbianFrameLoaderClient::dispatchDidReplaceStateWithinPage()
{
    dispatchDidChangeLocationWithinPage();
}

void SymbianFrameLoaderClient::dispatchDidPopStateWithinPage()
{
    dispatchDidChangeLocationWithinPage();
}

void SymbianFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    if (isMainFrame())
        m_view.progressChanged();
}

void SymbianFrameLoaderClient::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    if (isMainFrame())
        m_view.titleChanged(title.string);
}

void SymbianFrameLoaderClient::dispatchDidCommitLoad(std::optional<HasInsecureContent>, std::optional<UsedLegacyTLS>, std::optional<WasPrivateRelayed>)
{
    if (isMainFrame())
        m_view.urlChanged();
}

void SymbianFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError& error, WillContinueLoading willContinueLoading, WillInternallyHandleFailure willInternallyHandleFailure)
{
    if (!isMainFrame())
        return;
    m_view.progressChanged();
    if (error.isCancellation() || willContinueLoading == WillContinueLoading::Yes || willInternallyHandleFailure == WillInternallyHandleFailure::Yes)
        return;
    m_view.loadErrorPage(error);
}

void SymbianFrameLoaderClient::dispatchDidFailLoad(const ResourceError&)
{
    if (isMainFrame())
        m_view.progressChanged();
}

void SymbianFrameLoaderClient::dispatchDidFinishLoad()
{
    if (isMainFrame())
        m_view.progressChanged();
}

void SymbianFrameLoaderClient::dispatchDecidePolicyForResponse(const ResourceResponse& response, const ResourceRequest&, const String&, FramePolicyFunction&& function)
{
    // There are no downloads yet: what cannot be shown is not loaded.
    function(canShowMIMEType(response.mimeType()) ? PolicyAction::Use : PolicyAction::Ignore);
}

void SymbianFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest& request, FormState*, const String&, std::optional<HitTestResult>&&, FramePolicyFunction&& function)
{
    RefPtr mainFrame = m_frame ? &m_frame->rootFrame() : nullptr;
    auto newRequest = request.isolatedCopy();
    function(PolicyAction::Ignore);
    if (!mainFrame)
        return;
    callOnMainThread([frame = WTF::move(mainFrame), request = WTF::move(newRequest)]() mutable {
        if (frame->page())
            frame->loader().load(FrameLoadRequest(*frame, WTF::move(request)));
    });
}

void SymbianFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, const ResourceResponse&, FormState*, const String&, std::optional<NavigationIdentifier>, std::optional<HitTestResult>&&, bool, NavigationUpgradeToHTTPSBehavior, SandboxFlags, PolicyDecisionMode, FramePolicyFunction&& function)
{
    function(PolicyAction::Use);
}

void SymbianFrameLoaderClient::dispatchWillSubmitForm(FormState&, URL&&, String&&, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void SymbianFrameLoaderClient::committedLoad(DocumentLoader* loader, const SharedBuffer& data)
{
    loader->commitData(data);
}

bool SymbianFrameLoaderClient::shouldFallBack(const ResourceError& error) const
{
    return !error.isCancellation();
}

bool SymbianFrameLoaderClient::canShowMIMEType(const String& mimeType) const
{
    return MIMETypeRegistry::canShowMIMEType(mimeType);
}

String SymbianFrameLoaderClient::userAgent(const URL&) const
{
    return standardUserAgent();
}

void SymbianFrameLoaderClient::transitionToCommittedForNewPage(InitializingIframe)
{
    m_frame->createView(m_view.size(), std::nullopt, { }, { });
}

RefPtr<LocalFrame> SymbianFrameLoaderClient::createFrame(const AtomString& name, HTMLFrameOwnerElement& ownerElement)
{
    auto sandboxFlags = ownerElement.sandboxFlags();
    sandboxFlags.add(m_frame->effectiveSandboxFlags());
    auto referrerPolicy = ownerElement.referrerPolicy();
    if (referrerPolicy == ReferrerPolicy::EmptyString)
        referrerPolicy = m_frame->effectiveReferrerPolicy();
    auto childFrame = LocalFrame::createSubframe(*m_frame->page(), [this] (auto& frame, auto& loader) {
        return makeUniqueRefWithoutRefCountedCheck<SymbianFrameLoaderClient>(m_view, frame, loader);
    }, generateFrameIdentifier(), sandboxFlags, referrerPolicy, ownerElement, FrameTreeSyncData::create());
    childFrame->tree().setSpecifiedName(name);
    childFrame->init();
    if (!childFrame->page())
        return nullptr;
    return childFrame;
}

RefPtr<HistoryItem> SymbianFrameLoaderClient::createHistoryItemTree(bool clipAtTarget, BackForwardItemIdentifier itemID) const
{
    return m_frame->rootFrame().loader().history().createItemTree(*m_frame, clipAtTarget, itemID);
}

ObjectContentType SymbianFrameLoaderClient::objectContentType(const URL&, const String& mimeType)
{
    if (mimeType.isEmpty())
        return ObjectContentType::Frame;
    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return ObjectContentType::Image;
    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return ObjectContentType::Frame;
    return ObjectContentType::None;
}

Ref<FrameNetworkingContext> SymbianFrameLoaderClient::createNetworkingContext()
{
    return SymbianFrameNetworkingContext::create(m_frame);
}

void SymbianFrameLoaderClient::sendH2Ping(const URL& url, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&& completionHandler)
{
    completionHandler(makeUnexpected(internalError(url)));
}

} // namespace WebKitSymbian
