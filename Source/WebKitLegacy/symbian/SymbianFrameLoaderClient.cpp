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
#include <WebCore/FrameDestructionObserverInlines.h>
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
#include <WebCore/NavigationAction.h>
#include <WebCore/DocumentSecurityOrigin.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SubresourceLoader.h>
#include <WebCore/UserAgent.h>

namespace WebKitSymbian {

using namespace WebCore;

namespace {

class AuthenticationTextResult final : public TextResult {
public:
    void setText(Text text) final
    {
        // Native descriptors are bounded. Reject malformed/oversized results
        // from another embedder before constructing an engine String.
        if (text.length < 0 || text.length > 256 || (text.length && !text.characters))
            return;
        value = text.length ? String(std::span(reinterpret_cast<const UChar*>(text.characters), static_cast<size_t>(text.length))) : emptyString();
        wasSet = true;
    }
    String value;
    bool wasSet { false };
};

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

void SymbianFrameLoaderClient::cancelAuthenticationChallenge()
{
    if (!m_authenticationIdentifier)
        return;
    auto identifier = std::exchange(m_authenticationIdentifier, std::nullopt);
    m_view.client().cancelAuthenticationChallenge(identifier->toUInt64());
}

void SymbianFrameLoaderClient::dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier identifier, const ResourceError&)
{
    if (m_authenticationIdentifier == identifier)
        cancelAuthenticationChallenge();
}

void SymbianFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader* loader, ResourceLoaderIdentifier identifier, const AuthenticationChallenge& challenge)
{
    // Copy before entering AVKON's nested event loop: cancel() nullifies the
    // ResourceHandle's current challenge, and can release the loader's handle.
    auto protectedChallenge = challenge;
    RefPtr authenticationClient = protectedChallenge.authenticationClient();
    if (!authenticationClient)
        return;
    const auto& space = protectedChallenge.protectionSpace();
    auto scheme = space.authenticationScheme();
    if (!m_frame || !loader || m_authenticationIdentifier || space.isProxy()
        || protectedChallenge.failureResponse().httpStatusCode() != 401
        || !protectedChallenge.failureResponse().url().protocolIsInHTTPFamily()
        || (scheme != ProtectionSpace::AuthenticationScheme::HTTPBasic && scheme != ProtectionSpace::AuthenticationScheme::HTTPDigest)
        || protectedChallenge.previousFailureCount() >= 3) {
        authenticationClient->receivedRequestToContinueWithoutCredential(protectedChallenge);
        return;
    }

    Ref protectedFrame { *m_frame };
    Ref protectedLoader { *loader };
    auto origin = SecurityOrigin::create(protectedChallenge.failureResponse().url())->toString();
    AuthenticationTextResult username;
    AuthenticationTextResult password;
    m_authenticationIdentifier = identifier;
    bool accepted = m_view.client().runAuthenticationChallenge(identifier.toUInt64(), UTF16Text(origin).text(), protectedChallenge.previousFailureCount() + 1, username, password);
    // Never consult m_view after returning: the embedding may have destroyed
    // the view during the dialog. The retained frame keeps this client alive.
    bool isCurrent = m_authenticationIdentifier == identifier && protectedFrame->page()
        && protectedLoader->frame() == protectedFrame.ptr()
        && (protectedFrame->loader().documentLoader() == protectedLoader.ptr()
            || protectedFrame->loader().provisionalDocumentLoader() == protectedLoader.ptr());
    m_authenticationIdentifier.reset();
    if (!accepted || !isCurrent || !username.wasSet || !password.wasSet) {
        authenticationClient->receivedCancellation(protectedChallenge);
        return;
    }
    authenticationClient->receivedCredential(protectedChallenge, Credential(username.value, password.value, CredentialPersistence::None));
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
    cancelAuthenticationChallenge();
    if (isMainFrame())
        m_view.client().cancelAuthenticationChallenge(0);
    // A provisional navigation invalidates a gesture even when initiated by JS.
    m_view.cancelTouches();
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
    bool successful = response.httpStatusCode() >= 200 && response.httpStatusCode() < 300;
    if (response.isAttachment() || (successful && !canShowMIMEType(response.mimeType())))
        function(PolicyAction::Download);
    else
        function(canShowMIMEType(response.mimeType()) ? PolicyAction::Use : PolicyAction::Ignore);
}

void SymbianFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const NavigationAction& action, const ResourceRequest& request, FormState*, const String&, std::optional<HitTestResult>&&, FramePolicyFunction&& function)
{
    if (!action.downloadAttribute().isNull()) {
        function(PolicyAction::Download);
        return;
    }
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

void SymbianFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction& action, const ResourceRequest&, const ResourceResponse&, FormState*, const String&, std::optional<NavigationIdentifier>, std::optional<HitTestResult>&&, bool, NavigationUpgradeToHTTPSBehavior, SandboxFlags, PolicyDecisionMode, FramePolicyFunction&& function)
{
    // HTMLAnchorElement already rejected cross-origin download attributes;
    // PolicyChecker still enforces the frame's sandbox-download flag.
    function(action.downloadAttribute().isNull() ? PolicyAction::Use : PolicyAction::Download);
}

void SymbianFrameLoaderClient::startDownload(const ResourceRequest& request, const String& suggestedFilename, FromDownloadAttribute)
{
    if (!m_frame)
        return;
    RefPtr document = m_frame->document();
    m_view.downloads().start(m_frame->loader().networkingContext(), request,
        document ? RefPtr { document->protectedSecurityOrigin() } : nullptr, suggestedFilename);
}

void SymbianFrameLoaderClient::convertMainResourceLoadToDownload(DocumentLoader* loader, const ResourceRequest&, const ResourceResponse& response)
{
    m_view.downloads().convert(loader ? loader->mainResourceLoader() : nullptr, response);
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
