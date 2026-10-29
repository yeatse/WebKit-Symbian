/*
 * Copyright (C) 2004, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 * All rights reserved.
 * Copyright (C) 2017 NAVER Corp.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ResourceHandle.h"

#if OS(SYMBIAN) && USE(CURL)
#include "CredentialStorage.h"
#include "CookieJarDB.h"
#include "CurlContext.h"
#include "CurlRequest.h"
#include "DNS.h"
#include "DataURLDecoder.h"
#include "NetworkStorageSession.h"
#include "ResourceHandleInternal.h"
#include "SameSiteInfo.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include "SynchronousLoaderClient.h"
#include <wtf/MainThread.h>

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal()
{
    if (m_curlRequest)
        m_curlRequest->cancel();
    if (m_messageQueue)
        m_messageQueue->kill();
}

ResourceHandle::~ResourceHandle() = default;

bool ResourceHandle::start()
{
    if (d->m_context && !d->m_context->isValid())
        return false;
    d->m_startTime = MonotonicTime::now();
    if (firstRequest().url().protocolIsData()) {
        handleDataURL();
        return true;
    }
    if (!firstRequest().url().protocolIsInHTTPFamily() && !firstRequest().url().protocolIsFile()) {
        scheduleFailure(InvalidURLFailure);
        return true;
    }
    auto request = firstRequest();
    d->m_curlRequest = createCurlRequest(WTF::move(request));
    if (!d->m_user.isEmpty() || !d->m_password.isEmpty())
        d->m_curlRequest->setUserPass(d->m_user, d->m_password);
    else if (shouldUseCredentialStorage() && context() && context()->storageSession()) {
        d->m_initialCredential = context()->storageSession()->credentialStorage().get(d->m_partition, firstRequest().url());
        if (!d->m_initialCredential.isEmpty()) {
            d->m_curlRequest->setUserPass(d->m_initialCredential.user(), d->m_initialCredential.password());
            d->m_curlRequest->setAuthenticationScheme(ProtectionSpace::AuthenticationScheme::HTTPBasic);
        }
    }
    if (!d->m_defersLoading)
        d->m_curlRequest->resume();
    return true;
}

Ref<CurlRequest> ResourceHandle::createCurlRequest(ResourceRequest&& request)
{
    // A redirected request must get cookies for its new path, even on the same origin.
    request.removeHTTPHeaderField(HTTPHeaderName::Cookie);
    if (request.allowCookies() && context() && context()->storageSession()) {
        auto secure = request.url().protocolIs("https"_s) ? IncludeSecureCookies::Yes : IncludeSecureCookies::No;
        auto cookies = context()->storageSession()->cookieRequestHeaderFieldValue(request.firstPartyForCookies(), SameSiteInfo::create(request), request.url(), std::nullopt, std::nullopt, secure, ApplyTrackingPrevention::Yes, ShouldRelaxThirdPartyCookieBlocking::No, IsKnownCrossSiteTracker::No).first;
        if (!cookies.isEmpty())
            request.setHTTPHeaderField(HTTPHeaderName::Cookie, cookies);
    }
    auto result = CurlRequest::create(request, *this);
    result->setMessageQueue(RefPtr { d->m_messageQueue });
    result->setDefersLoading(d->m_defersLoading);
    return result;
}

void ResourceHandle::cancel()
{
    d->m_cancelled = true;
    d->m_failureTimer.stop();
    d->m_scheduledFailureType = NoFailure;
    clearAuthentication();
    if (d->m_curlRequest)
        d->m_curlRequest->cancel();
    d->m_deferredCallbacks.clear();
    if (d->m_messageQueue)
        d->m_messageQueue->kill();
}

bool ResourceHandle::acceptsCurlCallback(const CurlRequest& request) const
{
    return !d->m_cancelled && client() && d->m_curlRequest.get() == &request;
}

void ResourceHandle::dispatchCurlCallback(Function<void(ResourceHandle&)>&& callback)
{
    if (d->m_cancelled || !client())
        return;
    d->m_deferredCallbacks.append(WTF::move(callback));
    drainCurlCallbacks();
}

void ResourceHandle::drainCurlCallbacks()
{
    if (d->m_drainingCallbacks)
        return;
    Ref protectedThis { *this };
    d->m_drainingCallbacks = true;
    while (!d->m_cancelled && client() && !d->m_defersLoading && !d->m_deferredCallbacks.isEmpty()) {
        auto callback = d->m_deferredCallbacks.takeFirst();
        callback(*this);
    }
    d->m_drainingCallbacks = false;
}

void ResourceHandle::platformSetDefersLoading(bool defers)
{
    if (d->m_cancelled)
        return;
    if (d->m_curlRequest) {
        d->m_curlRequest->setDefersLoading(defers);
        if (!defers)
            d->m_curlRequest->resume();
    }
    if (!defers)
        drainCurlCallbacks();
}

void ResourceHandle::curlDidSendData(CurlRequest& request, unsigned long long sent, unsigned long long total)
{
    dispatchCurlCallback([request = Ref { request }, sent, total](ResourceHandle& handle) {
        if (handle.acceptsCurlCallback(request))
            handle.client()->didSendData(&handle, sent, total);
    });
}

void ResourceHandle::curlDidReceiveResponse(CurlRequest& request, CurlResponse&& response)
{
    dispatchCurlCallback([request = Ref { request }, response = WTF::move(response)](ResourceHandle& handle) mutable {
        if (!handle.acceptsCurlCallback(request))
            return;
        auto& state = *handle.d;
        state.m_response = ResourceResponse(response);
        handle.checkTAO(state.m_response);
        state.m_response.setDeprecatedNetworkLoadMetrics(Box<NetworkLoadMetrics>::create(WTF::move(response.networkLoadMetrics)));
        if (request->resourceRequest().allowCookies() && handle.context() && handle.context()->storageSession()) {
            auto sameSiteInfo = SameSiteInfo::create(request->resourceRequest());
            for (auto& header : response.headers) {
                if (header.startsWithIgnoringASCIICase("set-cookie:"_s))
                    handle.context()->storageSession()->cookieDatabase().setCookie(request->resourceRequest().firstPartyForCookies(), response.url, header.substring(11).trim(deprecatedIsSpaceOrNewline), CookieJarDB::Source::Network, std::nullopt, &sameSiteInfo);
            }
        }
        auto status = state.m_response.httpStatusCode();
        auto location = state.m_response.httpHeaderField(HTTPHeaderName::Location);
        if ((status == 301 || status == 302 || status == 303 || status == 307 || status == 308) && !location.isEmpty()) {
            if (++state.m_redirectCount > 20) {
                handle.cancel();
                handle.client()->didFail(&handle, ResourceError(CURLE_TOO_MANY_REDIRECTS, response.url));
                return;
            }
            auto redirected = request->resourceRequest();
            URL target { response.url, location };
            redirected.setURL(URL(target));
            if ((status == 303 && redirected.httpMethod() != "HEAD"_s) || ((status == 301 || status == 302) && redirected.httpMethod() == "POST"_s)) {
                redirected.setHTTPMethod("GET"_s);
                redirected.setHTTPBody(nullptr);
                redirected.removeHTTPHeaderField(HTTPHeaderName::ContentType);
                redirected.removeHTTPHeaderField(HTTPHeaderName::ContentLength);
                redirected.removeHTTPHeaderField(HTTPHeaderName::ContentEncoding);
                redirected.removeHTTPHeaderField(HTTPHeaderName::TransferEncoding);
            }
            if (!protocolHostAndPortAreEqual(response.url, target)) {
                redirected.clearHTTPAuthorization();
                redirected.removeHTTPHeaderField(HTTPHeaderName::Cookie);
                redirected.clearHTTPOrigin();
            }
            if (!target.protocolIs("https"_s) && protocolIs(redirected.httpReferrer(), "https"_s) && handle.context() && handle.context()->shouldClearReferrerOnHTTPSToHTTPRedirect())
                redirected.clearHTTPReferrer();
            redirected.removeCredentials();
            handle.client()->willSendRequestAsync(&handle, WTF::move(redirected), ResourceResponse(state.m_response), [protectedHandle = Ref { handle }, request, previousURL = response.url](ResourceRequest&& next) mutable {
                if (protectedHandle->acceptsCurlCallback(request))
                    protectedHandle->continueCurlRedirect(WTF::move(next), previousURL);
            });
            return;
        }
        if (state.m_response.isUnauthorized() && response.availableHttpAuth && state.m_authFailureCount < 3) {
            AuthenticationChallenge challenge(response, state.m_authFailureCount++, state.m_response);
            challenge.setAuthenticationClient(handle);
            handle.didReceiveAuthenticationChallenge(challenge);
            return;
        }
        handle.deliverCurlResponse();
    });
}

void ResourceHandle::continueCurlRedirect(ResourceRequest&& request, const URL& previousURL)
{
    if (request.isNull()) {
        cancel();
        return;
    }
    if (!request.url().isValid() || !portAllowed(request.url()) || isIPAddressDisallowed(request.url()) || (previousURL.protocolIsInHTTPFamily() && !request.url().protocolIsInHTTPFamily())) {
        cancel();
        client()->didFail(this, ResourceError(CURLE_UNSUPPORTED_PROTOCOL, request.url()));
        return;
    }
    bool sameOrigin = protocolHostAndPortAreEqual(previousURL, request.url());
    // The loader handles ordinary subresource redirects too. Keep this boundary
    // conservative for every Legacy client: A -> B -> A must not regain Strict.
    if (request.isSameSite() && !SecurityOrigin::create(previousURL)->isSameSiteAs(SecurityOrigin::create(request.url())))
        request.setIsSameSite(false);
    String user = sameOrigin ? d->m_curlRequest->user() : String();
    String password = sameOrigin ? d->m_curlRequest->password() : String();
    if (!sameOrigin) {
        request.clearHTTPAuthorization();
        request.removeHTTPHeaderField(HTTPHeaderName::Cookie);
        // The client has already applied CORS redirect rules, including a new
        // or opaque Origin. Preserve that value when continuing this request.
        d->m_user = { };
        d->m_password = { };
        d->m_initialCredential = { };
        d->m_hasCrossOriginRedirect = true;
    }
    request.removeCredentials();
    d->m_lastHTTPMethod = request.httpMethod();
    d->m_curlRequest->cancel();
    d->m_curlRequest = createCurlRequest(WTF::move(request));
    if (!user.isEmpty() || !password.isEmpty())
        d->m_curlRequest->setUserPass(user, password);
    if (!d->m_defersLoading)
        d->m_curlRequest->resume();
}

void ResourceHandle::deliverCurlResponse()
{
    RefPtr request = d->m_curlRequest;
    if (!request || !acceptsCurlCallback(*request))
        return;
    didReceiveResponse(ResourceResponse(d->m_response), [protectedThis = Ref { *this }, request] {
        if (protectedThis->acceptsCurlCallback(*request))
            request->completeDidReceiveResponse();
    });
}

void ResourceHandle::curlDidReceiveData(CurlRequest& request, Ref<SharedBuffer>&& buffer)
{
    dispatchCurlCallback([request = Ref { request }, buffer = WTF::move(buffer)](ResourceHandle& handle) {
        if (handle.acceptsCurlCallback(request))
            handle.client()->didReceiveBuffer(&handle, buffer, buffer->size());
    });
}

void ResourceHandle::curlDidComplete(CurlRequest& request, NetworkLoadMetrics&& metrics)
{
    dispatchCurlCallback([request = Ref { request }, metrics = WTF::move(metrics)](ResourceHandle& handle) {
        if (handle.acceptsCurlCallback(request)) {
            handle.cancel();
            handle.client()->didFinishLoading(&handle, metrics);
        }
    });
}

void ResourceHandle::curlDidFailWithError(CurlRequest& request, ResourceError&& error, CertificateInfo&&)
{
    dispatchCurlCallback([request = Ref { request }, error = WTF::move(error)](ResourceHandle& handle) {
        if (handle.acceptsCurlCallback(request)) {
            handle.cancel();
            handle.client()->didFail(&handle, error);
        }
    });
}

bool ResourceHandle::shouldUseCredentialStorage()
{
    return client() && client()->shouldUseCredentialStorage(this) && firstRequest().url().protocolIsInHTTPFamily();
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    d->m_currentWebChallenge = challenge;
    if (client())
        client()->didReceiveAuthenticationChallenge(this, challenge);
    else
        receivedRequestToContinueWithoutCredential(challenge);
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    if (d->m_cancelled || !client() || !AuthenticationChallenge::equalForWebKitLegacyChallengeComparison(challenge, d->m_currentWebChallenge) || !d->m_curlRequest)
        return;
    if (credential.isEmpty()) {
        receivedRequestToContinueWithoutCredential(challenge);
        return;
    }
    if (shouldUseCredentialStorage() && context() && context()->storageSession())
        context()->storageSession()->credentialStorage().set(d->m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
    auto request = d->m_curlRequest->resourceRequest();
    d->m_curlRequest->cancel();
    d->m_curlRequest = createCurlRequest(WTF::move(request));
    d->m_curlRequest->setUserPass(credential.user(), credential.password());
    d->m_curlRequest->setAuthenticationScheme(challenge.protectionSpace().authenticationScheme());
    clearAuthentication();
    if (!d->m_defersLoading)
        d->m_curlRequest->resume();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    if (!AuthenticationChallenge::equalForWebKitLegacyChallengeComparison(challenge, d->m_currentWebChallenge))
        return;
    clearAuthentication();
    deliverCurlResponse();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    if (!AuthenticationChallenge::equalForWebKitLegacyChallengeComparison(challenge, d->m_currentWebChallenge))
        return;
    if (client())
        client()->receivedCancellation(this, challenge);
    cancel();
}

void ResourceHandle::receivedRequestToPerformDefaultHandling(const AuthenticationChallenge& challenge)
{
    receivedRequestToContinueWithoutCredential(challenge);
}

void ResourceHandle::receivedChallengeRejection(const AuthenticationChallenge& challenge)
{
    receivedRequestToContinueWithoutCredential(challenge);
}

void ResourceHandle::handleDataURL()
{
    auto task = [protectedThis = Ref { *this }] {
        protectedThis->dispatchCurlCallback([](ResourceHandle& handle) {
            auto result = DataURLDecoder::decode(handle.firstRequest().url());
            if (!result) {
                handle.client()->didFail(&handle, ResourceError(CURLE_URL_MALFORMAT, handle.firstRequest().url()));
                return;
            }
            auto buffer = SharedBuffer::create(WTF::move(result->data));
            ResourceResponse response(URL(handle.firstRequest().url()), WTF::move(result->mimeType), buffer->size(), WTF::move(result->charset));
            handle.didReceiveResponse(WTF::move(response), [protectedHandle = Ref { handle }, buffer = WTF::move(buffer)]() mutable {
                protectedHandle->dispatchCurlCallback([buffer = WTF::move(buffer)](ResourceHandle& handle) {
                    handle.client()->didReceiveBuffer(&handle, buffer, buffer->size());
                    handle.dispatchCurlCallback([](ResourceHandle& handle) {
                        handle.cancel();
                        handle.client()->didFinishLoading(&handle, { });
                    });
                });
            });
        });
    };
    if (d->m_messageQueue)
        d->m_messageQueue->append(makeUnique<Function<void()>>(WTF::move(task)));
    else
        callOnMainThread(WTF::move(task));
}

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext* context, const ResourceRequest& request, StoredCredentialsPolicy credentials, SecurityOrigin* origin, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    SynchronousLoaderClient client;
    client.setAllowStoredCredentials(credentials == StoredCredentialsPolicy::Use);
    Ref handle = adoptRef(*new ResourceHandle(context, request, &client, false, true, ContentEncodingSniffingPolicy::Default, RefPtr { origin }, false));
    handle->d->m_messageQueue = &client.messageQueue();
    if (handle->d->m_scheduledFailureType != NoFailure || (!request.url().protocolIsInHTTPFamily() && !request.url().protocolIsFile() && !request.url().protocolIsData()) || !handle->start()) {
        error = ResourceError(CURLE_URL_MALFORMAT, request.url());
        handle->cancel();
        return;
    }
    while (!client.messageQueue().killed() && !handle->d->m_cancelled) {
        if (auto task = client.messageQueue().waitForMessage())
            (*task)();
    }
    error = client.error();
    response = client.response();
    data.swap(client.mutableData());
    handle->cancel();
    handle->clearClient();
}

} // namespace WebCore
#endif
