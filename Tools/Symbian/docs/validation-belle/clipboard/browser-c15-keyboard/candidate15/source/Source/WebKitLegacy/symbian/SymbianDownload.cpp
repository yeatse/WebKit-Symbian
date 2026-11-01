/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "SymbianDownload.h"

#include "DownloadFilename.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/NetworkingContext.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/MainThread.h>
#include <wtf/text/MakeString.h>

namespace WebKitSymbian {
using namespace WebCore;

namespace {
class DownloadNetworkingContext final : public NetworkingContext {
public:
    static Ref<DownloadNetworkingContext> create(NetworkingContext& source)
    {
        return adoptRef(*new DownloadNetworkingContext(source));
    }

private:
    explicit DownloadNetworkingContext(NetworkingContext& source)
        : m_storageProvider(source)
    {
    }
    NetworkStorageSession* storageSession() const final { return m_storageProvider->storageSession(); }
    // A download can outlive its initiating document. Never consult that
    // document (or a replacement document) during a later redirect, and never
    // send a secure referrer over an insecure redirected connection.
    bool shouldClearReferrerOnHTTPSToHTTPRedirect() const final { return true; }
    Ref<StorageSessionProvider> m_storageProvider;
};
}

Ref<SymbianDownload> SymbianDownload::create(SymbianDownloadClient& client, String suggestedFilename)
{
    return adoptRef(*new SymbianDownload(client, WTF::move(suggestedFilename)));
}

SymbianDownload::SymbianDownload(SymbianDownloadClient& client, String&& filename)
    : m_client(&client)
    , m_suggestedFilename(WTF::move(filename))
{
}

SymbianDownload::~SymbianDownload()
{
    m_client = nullptr;
    stopNetwork();
    if (m_file)
        m_file->cancel();
}

void SymbianDownload::start(NetworkingContext* context, const ResourceRequest& request, RefPtr<SecurityOrigin>&& origin)
{
    ASSERT(isMainThread());
    Ref protectedThis { *this };
    if (m_state != State::Created)
        return;
    m_source = request.url();
    if (!m_source.protocolIsInHTTPFamily())
        return finish({ DownloadFailure::Unsupported, "Only HTTP and HTTPS downloads are supported"_s });
    if (context && !context->isValid())
        return finish({ DownloadFailure::Cancelled, "The initiating frame is no longer available"_s });
    RefPtr<NetworkingContext> downloadContext;
    if (context)
        downloadContext = DownloadNetworkingContext::create(*context);
    m_state = State::AwaitingResponse;
    m_handle = ResourceHandle::create(downloadContext.get(), request, this, true, false,
        ContentEncodingSniffingPolicy::Default, WTF::move(origin), request.isTopSite());
    if (!m_handle)
        return finish({ DownloadFailure::Network, "Unable to start the download"_s });
    m_handle->setDownloadBufferLimit(maximumPendingDataBytes);
    m_handle->setDefersLoading(false);
}

void SymbianDownload::convert(ResourceLoader& loader, const ResourceResponse& response)
{
    ASSERT(isMainThread());
    Ref protectedThis { *this };
    if (m_state != State::Created)
        return;
    m_source = response.url();
    if (!m_source.protocolIsInHTTPFamily())
        return finish({ DownloadFailure::Unsupported, "This response cannot be converted to a download"_s });
    m_handle = loader.takeHandleForDownload(*this, maximumPendingDataBytes);
    if (!m_handle)
        return finish({ DownloadFailure::Unsupported, "No live network response is available; the request was not repeated"_s });
    m_state = State::AwaitingResponse;
    receiveResponse(response);
}

void SymbianDownload::willSendRequestAsync(ResourceHandle*, ResourceRequest&& request, ResourceResponse&&,
    CompletionHandler<void(ResourceRequest&&)>&& continuation)
{
    Ref protectedThis { *this };
    // ResourceHandle keeps the original cookie context, redirect count,
    // cross-origin credential stripping and strict peer/hostname verification.
    // Never authorize a network redirect into a local/non-network protocol.
    if (finished() || !request.url().protocolIsInHTTPFamily()) {
        continuation({ });
        if (!finished())
            finish({ DownloadFailure::Unsupported, "Download redirect used an unsupported protocol"_s });
        return;
    }
    m_source = request.url();
    continuation(WTF::move(request));
}

void SymbianDownload::didReceiveResponseAsync(ResourceHandle*, ResourceResponse&& response, CompletionHandler<void()>&& continuation)
{
    Ref protectedThis { *this };
    if (finished()) {
        continuation();
        return;
    }
    m_continueResponse = WTF::move(continuation);
    receiveResponse(response);
}

void SymbianDownload::receiveResponse(const ResourceResponse& response)
{
    if (response.httpStatusCode() < 200 || response.httpStatusCode() >= 300 || response.httpStatusCode() == 206)
        return finish({ DownloadFailure::HTTPStatus, "The server did not return a complete successful response"_s, response.httpStatusCode() });
    if (response.isMultipart())
        return finish({ DownloadFailure::Unsupported, "Multipart downloads are not supported"_s });
    m_source = response.url();
    // libcurl checks encoded Content-Length and transfer framing. Only an
    // explicit unencoded, unframed Content-Length describes bytes we write.
    // In particular, unknown is not zero, including HTTP/2 without this field.
    m_total = std::nullopt;
    if (response.httpHeaderField(HTTPHeaderName::ContentEncoding).isEmpty()
        && response.httpHeaderField(HTTPHeaderName::TransferEncoding).isEmpty()
        && !response.httpHeaderField(HTTPHeaderName::ContentLength).isEmpty()
        && response.expectedContentLength() >= 0)
        m_total = static_cast<uint64_t>(response.expectedContentLength());
    if (m_total && *m_total > DownloadFile::maximumSize)
        return finish({ DownloadFailure::SizeLimit, "The download exceeds the supported 2 GiB minus one byte limit"_s });
    String name = response.suggestedFilename();
    if (name.isEmpty())
        name = m_suggestedFilename;
    if (name.isEmpty())
        name = m_source.lastPathComponent().toString();
    uint16_t buffer[128];
    unsigned length = name.is8Bit()
        ? makeDownloadFilename(name.span8().data(), name.length(), buffer, 128)
        : makeDownloadFilename(name.span16().data(), name.length(), buffer, 128);
    char16_t stringBuffer[128];
    for (unsigned i = 0; i < length; ++i)
        stringBuffer[i] = buffer[i];
    m_suggestedFilename = String(std::span { stringBuffer, length });
    m_state = State::ChoosingDestination;
    m_handle->setDefersLoading(true);
    // Do not enter a native modal loop inside the document's content-policy
    // callback. Let the old loader finish its policy-change teardown first.
    callOnMainThread([protectedThis = Ref { *this }] {
        if (protectedThis->m_state != State::ChoosingDestination || !protectedThis->m_client)
            return;
        protectedThis->m_client->chooseDownloadDestination(protectedThis, protectedThis->m_source,
            protectedThis->m_suggestedFilename, [protectedThis](String&& path) mutable {
                protectedThis->chooseDestination(WTF::move(path));
            });
    });
}

void SymbianDownload::chooseDestination(String&& path)
{
    ASSERT(isMainThread());
    Ref protectedThis { *this };
    if (m_state != State::ChoosingDestination || !m_client)
        return;
    if (path.isEmpty())
        return cancel();
    if (path.length() > 256)
        return finish({ DownloadFailure::Destination, "The selected download path is too long"_s });
    uint16_t nativePath[256];
    for (unsigned i = 0; i < path.length(); ++i)
        nativePath[i] = path[i];
    if (!isValidDownloadDestination(nativePath, path.length()))
        return finish({ DownloadFailure::Destination, "The selected download path is invalid"_s });
    m_file = m_client->createDownloadFile();
    if (finished() || !m_client)
        return;
    if (!m_file)
        return finish({ DownloadFailure::Destination, "The application has no download file backend"_s });
    int error = m_file->open(nativePath, path.length());
    if (error)
        return finish({ DownloadFailure::Destination, "Unable to create an exclusive download file; existing files are never replaced"_s, error });
    m_destination = WTF::move(path);
    m_state = State::Receiving;
    if (auto continuation = std::exchange(m_continueResponse, { }))
        continuation();
    if (finished())
        return;
    m_handle->setDefersLoading(false);
    reportProgress();
}

void SymbianDownload::didReceiveBuffer(ResourceHandle*, const FragmentedSharedBuffer& buffer, int)
{
    Ref protectedThis { *this };
    if (m_state != State::Receiving)
        return;
    if (buffer.size() > DownloadFile::maximumSize - m_received)
        return finish({ DownloadFailure::SizeLimit, "The download exceeds the supported 2 GiB minus one byte limit"_s });
    int error = 0;
    buffer.forEachSegment([&](std::span<const uint8_t> segment) {
        if (!error)
            error = m_file->append(segment.data(), segment.size());
    });
    if (error)
        return finish({ DownloadFailure::Write, "Unable to write the download (check disk space and media availability)"_s, error });
    m_received += buffer.size();
    reportProgress();
}

void SymbianDownload::reportProgress()
{
    if (finished() || !m_client)
        return;
    auto now = MonotonicTime::now();
    if (m_lastProgress && now - m_lastProgress < 100_ms)
        return;
    m_lastProgress = now;
    m_client->downloadProgress(*this, m_received, m_total);
}

void SymbianDownload::didFinishLoading(ResourceHandle*, const NetworkLoadMetrics&)
{
    Ref protectedThis { *this };
    if (finished())
        return;
    if (m_state != State::Receiving || !m_file)
        return finish({ DownloadFailure::Network, "The response ended before the download destination was ready"_s });
    if (m_total && m_received != *m_total)
        return finish({ DownloadFailure::Network, "The received size does not match Content-Length"_s });
    int error = m_file->finish();
    if (error)
        return finish({ DownloadFailure::Finish, "Unable to flush or publish the download; the destination was not replaced"_s, error });
    finish({ });
}

void SymbianDownload::didFail(ResourceHandle*, const ResourceError& error)
{
    finish({ DownloadFailure::Network, error.localizedDescription(), error.errorCode() });
}

void SymbianDownload::wasBlocked(ResourceHandle*)
{
    finish({ DownloadFailure::Network, "The download address or port was blocked"_s });
}

void SymbianDownload::cannotShowURL(ResourceHandle*)
{
    finish({ DownloadFailure::Unsupported, "The download URL is unsupported"_s });
}

void SymbianDownload::didReceiveAuthenticationChallenge(ResourceHandle* handle, const AuthenticationChallenge& challenge)
{
    // Match the port's current authentication policy; never invent credentials
    // or disable certificate validation to turn a failure into a download.
    handle->receivedRequestToContinueWithoutCredential(challenge);
}

void SymbianDownload::stopNetwork()
{
    if (auto handle = std::exchange(m_handle, nullptr)) {
        handle->cancel();
        handle->clearClient();
    }
    if (auto continuation = std::exchange(m_continueResponse, { }))
        continuation();
}

void SymbianDownload::finish(DownloadResult&& result)
{
    Ref protectedThis { *this };
    if (finished())
        return;
    m_state = State::Finished;
    stopNetwork();
    if (m_file) {
        result.cleanupError = m_file->cancel();
        m_file = nullptr;
    }
    if (auto* client = std::exchange(m_client, nullptr))
        client->downloadFinished(*this, m_destination, result);
}

void SymbianDownload::cancel()
{
    finish({ DownloadFailure::Cancelled, "Download cancelled"_s });
}

void SymbianDownload::destinationFailed(int nativeError)
{
    if (m_state == State::ChoosingDestination)
        finish({ DownloadFailure::Destination, "The native save dialog could not select a destination"_s, nativeError });
}

void SymbianDownload::detachClient()
{
    m_client = nullptr;
    cancel();
}

} // namespace WebKitSymbian
