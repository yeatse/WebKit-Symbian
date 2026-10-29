/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include "SocketStreamHandle.h"
#include <WebCore/CurlStream.h>
#include <WebCore/StorageSessionProvider.h>
#include <pal/SessionID.h>

namespace WebCore {

class SocketStreamHandleImpl final : public SocketStreamHandle, private CurlStream::Client {
public:
    static Ref<SocketStreamHandleImpl> create(const URL& url, SocketStreamHandleClient& client, PAL::SessionID sessionID, const String& partition, SourceApplicationAuditToken&& audit, const StorageSessionProvider* provider, bool acceptInsecureCertificates)
    {
        return adoptRef(*new SocketStreamHandleImpl(url, client, sessionID, partition, WTF::move(audit), provider, acceptInsecureCertificates));
    }
    ~SocketStreamHandleImpl();
    static void setLegacyTLSEnabled(bool) { }
    size_t bufferedAmount() final { return m_bufferedAmount; }

private:
    SocketStreamHandleImpl(const URL&, SocketStreamHandleClient&, PAL::SessionID, const String&, SourceApplicationAuditToken&&, const StorageSessionProvider*, bool);
    void platformSend(std::span<const uint8_t>, Function<void(bool)>&&) final;
    void platformSendHandshake(std::span<const uint8_t>, const std::optional<CookieRequestHeaderFieldProxy>&, Function<void(bool, bool)>&&) final;
    void platformClose() final;
    void didOpen(CurlStreamID) final;
    void didSendData(CurlStreamID, size_t) final;
    void didReceiveData(CurlStreamID, const SharedBuffer&) final;
    void didFail(CurlStreamID, CURLcode, CertificateInfo&&) final;
    CurlStreamID m_streamID { invalidCurlStreamID };
    size_t m_bufferedAmount { 0 };
    RefPtr<const StorageSessionProvider> m_storageSessionProvider;
    static constexpr size_t maxBufferSize = 16 * 1024 * 1024;
};

}
