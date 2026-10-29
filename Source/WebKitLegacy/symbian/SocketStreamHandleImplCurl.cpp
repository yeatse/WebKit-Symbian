/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "SocketStreamHandleImplCurl.h"
#include "SocketStreamHandleClient.h"
#include <WebCore/CookieRequestHeaderFieldProxy.h>
#include <WebCore/CurlStreamScheduler.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SocketStreamError.h>
#include <wtf/Function.h>
#include <wtf/MainThread.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

SocketStreamHandleImpl::SocketStreamHandleImpl(const URL& url, SocketStreamHandleClient& client, PAL::SessionID, const String&, SourceApplicationAuditToken&&, const StorageSessionProvider* provider, bool)
    : SocketStreamHandle(url, client)
    , m_storageSessionProvider(provider)
{
    // The Legacy API's insecure-certificate switch must not relax Symbian TLS.
    m_streamID = CurlContext::singleton().streamScheduler().createStream(url, *this, CurlStream::ServerTrustEvaluation::Enable, CurlStream::LocalhostAlias::Disable);
}

SocketStreamHandleImpl::~SocketStreamHandleImpl()
{
    if (m_streamID != invalidCurlStreamID)
        CurlContext::singleton().streamScheduler().destroyStream(m_streamID);
}

void SocketStreamHandleImpl::platformSend(std::span<const uint8_t> data, Function<void(bool)>&& completion)
{
    if (m_state != Open || m_streamID == invalidCurlStreamID || data.size() > maxBufferSize - m_bufferedAmount) {
        completion(false);
        return;
    }
    Ref protectedThis { *this };
    if (!data.empty()) {
        auto copy = makeUniqueArray<uint8_t>(data.size());
        memcpy(copy.get(), data.data(), data.size());
        m_bufferedAmount += data.size();
        CurlContext::singleton().streamScheduler().send(m_streamID, WTF::move(copy), data.size());
        m_client.didUpdateBufferedAmount(*this, m_bufferedAmount);
    }
    completion(true);
}

void SocketStreamHandleImpl::platformSendHandshake(std::span<const uint8_t> data, const std::optional<CookieRequestHeaderFieldProxy>& proxy, Function<void(bool, bool)>&& completion)
{
    bool secureCookiesAccessed = false;
    Vector<uint8_t> handshake;
    if (proxy) {
        auto* session = m_storageSessionProvider ? m_storageSessionProvider->storageSession() : nullptr;
        if (!session || data.size() < 2 || data[data.size() - 2] != '\r' || data.back() != '\n') {
            completion(false, false);
            return;
        }
        auto cookies = session->cookieRequestHeaderFieldValue(*proxy);
        secureCookiesAccessed = cookies.second;
        if (!cookies.first.isEmpty()) {
            handshake.append(data.first(data.size() - 2));
            auto header = makeString("Cookie: "_s, cookies.first, "\r\n\r\n"_s).utf8();
            handshake.append(byteCast<uint8_t>(header.span()));
            data = handshake.span();
        }
    }
    platformSend(data, [completion = WTF::move(completion), secureCookiesAccessed](bool result) mutable {
        completion(result, secureCookiesAccessed);
    });
}

void SocketStreamHandleImpl::platformClose()
{
    if (m_state == Closed)
        return;
    Ref protectedThis { *this };
    m_state = Closed;
    if (m_streamID != invalidCurlStreamID)
        CurlContext::singleton().streamScheduler().destroyStream(std::exchange(m_streamID, invalidCurlStreamID));
    m_bufferedAmount = 0;
    m_client.didCloseSocketStream(*this);
}

void SocketStreamHandleImpl::didOpen(CurlStreamID identifier)
{
    if (identifier != m_streamID || m_state != Connecting)
        return;
    Ref protectedThis { *this };
    m_state = Open;
    m_client.didOpenSocketStream(*this);
}

void SocketStreamHandleImpl::didSendData(CurlStreamID identifier, size_t length)
{
    if (identifier != m_streamID)
        return;
    Ref protectedThis { *this };
    RELEASE_ASSERT(length <= m_bufferedAmount);
    m_bufferedAmount -= length;
    m_client.didUpdateBufferedAmount(*this, m_bufferedAmount);
    if (!m_bufferedAmount && m_state == Closing)
        disconnect();
}

void SocketStreamHandleImpl::didReceiveData(CurlStreamID identifier, const SharedBuffer& buffer)
{
    if (identifier != m_streamID || m_state == Closed)
        return;
    Ref protectedThis { *this };
    if (buffer.isEmpty()) {
        disconnect();
        return;
    }
    m_client.didReceiveSocketStreamData(*this, buffer.span());
}

void SocketStreamHandleImpl::didFail(CurlStreamID identifier, CURLcode error, CertificateInfo&&)
{
    if (identifier != m_streamID || m_state == Closed)
        return;
    Ref protectedThis { *this };
    m_client.didFailSocketStream(*this, SocketStreamError(error, m_url.string(), CurlHandle::errorDescription(error)));
    disconnect();
}

}
