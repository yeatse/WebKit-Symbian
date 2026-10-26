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
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameNetworkingContext.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/Page.h>
#include <WebCore/PolicyChecker.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/UserAgent.h>

namespace WebKitSymbian {

using namespace WebCore;

namespace {

const char* const errorDomain = "WebKitSymbian";

enum ErrorCode {
    Cancelled = -999,
    Blocked = 101,
    CannotShowURL = 102,
    InterruptedForPolicyChange = 103,
    CannotShowMIMEType = 104,
    FileDoesNotExist = 105,
    PluginWillHandleLoad = 106,
};

class SymbianFrameNetworkingContext final : public FrameNetworkingContext {
public:
    static Ref<SymbianFrameNetworkingContext> create(Frame* frame)
    {
        return adoptRef(*new SymbianFrameNetworkingContext(frame));
    }

private:
    explicit SymbianFrameNetworkingContext(Frame* frame)
        : FrameNetworkingContext(frame)
    {
    }

    NetworkStorageSession* storageSession() const final { return &NetworkStorageSessionMap::defaultStorageSession(); }
};

} // namespace

SymbianFrameLoaderClient::SymbianFrameLoaderClient(WebViewImpl& view)
    : m_view(view)
{
}

bool SymbianFrameLoaderClient::isMainFrame() const
{
    return m_frame && m_frame->isMainFrame();
}

Ref<DocumentLoader> SymbianFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    return DocumentLoader::create(request, substituteData);
}

void SymbianFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge& challenge)
{
    // FIXME: Ask for credentials.
    challenge.authenticationClient()->receivedRequestToContinueWithoutCredential(challenge);
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

void SymbianFrameLoaderClient::dispatchDidCommitLoad(Optional<HasInsecureContent>, Optional<UsedLegacyTLS>)
{
    if (isMainFrame())
        m_view.urlChanged();
}

void SymbianFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError& error, WillContinueLoading willContinueLoading)
{
    if (!isMainFrame())
        return;
    m_view.progressChanged();
    if (error.isCancellation() || willContinueLoading == WillContinueLoading::Yes)
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

void SymbianFrameLoaderClient::dispatchDecidePolicyForResponse(const ResourceResponse& response, const ResourceRequest&, PolicyCheckIdentifier identifier, const String&, FramePolicyFunction&& function)
{
    // There are no downloads yet: what cannot be shown is not loaded.
    function(canShowMIMEType(response.mimeType()) ? PolicyAction::Use : PolicyAction::Ignore, identifier);
}

void SymbianFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest& request, FormState*, const String&, PolicyCheckIdentifier identifier, FramePolicyFunction&& function)
{
    // One view: links that would open a window replace the current page.
    function(PolicyAction::Ignore, identifier);
    if (!m_frame)
        return;
    Frame& mainFrame = m_frame->mainFrame();
    callOnMainThread([frame = makeRef(mainFrame), request = request.isolatedCopy()]() mutable {
        frame->loader().load(FrameLoadRequest(frame.get(), request));
    });
}

void SymbianFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, const ResourceResponse&, FormState*, PolicyDecisionMode, PolicyCheckIdentifier identifier, FramePolicyFunction&& function)
{
    function(PolicyAction::Use, identifier);
}

void SymbianFrameLoaderClient::dispatchWillSubmitForm(FormState&, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void SymbianFrameLoaderClient::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    loader->commitData(data, length);
}

ResourceError SymbianFrameLoaderClient::cancelledError(const ResourceRequest& request) const
{
    return ResourceError(errorDomain, Cancelled, request.url(), "Cancelled", ResourceError::Type::Cancellation);
}

ResourceError SymbianFrameLoaderClient::blockedError(const ResourceRequest& request) const
{
    return ResourceError(errorDomain, Blocked, request.url(), "Not allowed to use restricted network port");
}

ResourceError SymbianFrameLoaderClient::blockedByContentBlockerError(const ResourceRequest& request) const
{
    return ResourceError(errorDomain, Blocked, request.url(), "Blocked by a content blocker");
}

ResourceError SymbianFrameLoaderClient::cannotShowURLError(const ResourceRequest& request) const
{
    return ResourceError(errorDomain, CannotShowURL, request.url(), "The URL can't be shown");
}

ResourceError SymbianFrameLoaderClient::interruptedForPolicyChangeError(const ResourceRequest& request) const
{
    return ResourceError(errorDomain, InterruptedForPolicyChange, request.url(), "Frame load interrupted", ResourceError::Type::Cancellation);
}

ResourceError SymbianFrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse& response) const
{
    return ResourceError(errorDomain, CannotShowMIMEType, response.url(), "Content with the specified MIME type can't be shown");
}

ResourceError SymbianFrameLoaderClient::fileDoesNotExistError(const ResourceResponse& response) const
{
    return ResourceError(errorDomain, FileDoesNotExist, response.url(), "File does not exist");
}

ResourceError SymbianFrameLoaderClient::pluginWillHandleLoadError(const ResourceResponse& response) const
{
    return ResourceError(errorDomain, PluginWillHandleLoad, response.url(), "Plug-in handled load");
}

bool SymbianFrameLoaderClient::shouldFallBack(const ResourceError& error) const
{
    return !error.isCancellation() && error.errorCode() != PluginWillHandleLoad;
}

bool SymbianFrameLoaderClient::canShowMIMEType(const String& mimeType) const
{
    return MIMETypeRegistry::canShowMIMEType(mimeType);
}

String SymbianFrameLoaderClient::userAgent(const URL&) const
{
    return standardUserAgent();
}

void SymbianFrameLoaderClient::transitionToCommittedForNewPage()
{
    m_frame->createView(m_view.size(), WTF::nullopt, { }, { });
}

RefPtr<Frame> SymbianFrameLoaderClient::createFrame(const String& name, HTMLFrameOwnerElement& ownerElement)
{
    auto client = makeUniqueRef<SymbianFrameLoaderClient>(m_view);
    auto& childClient = client.get();
    auto childFrame = Frame::create(m_frame->page(), &ownerElement, WTFMove(client));
    childClient.setFrame(childFrame.get());
    childFrame->tree().setName(name);
    m_frame->tree().appendChild(childFrame);
    childFrame->init();
    // init() can run script that removes the frame again.
    if (!childFrame->page())
        return nullptr;
    return childFrame;
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
