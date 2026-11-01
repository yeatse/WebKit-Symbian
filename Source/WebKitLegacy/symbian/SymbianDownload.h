/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "DownloadFile.h"
#include <WebCore/ResourceHandleClient.h>
#include <WebCore/Timer.h>
#include <wtf/CompletionHandler.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>
#include <memory>
#include <optional>

namespace WebCore {
class NetworkingContext;
class ResourceLoader;
class SecurityOrigin;
}

namespace WebKitSymbian {

class SymbianDownload;

struct DownloadResult {
    DownloadFailure failure { DownloadFailure::None };
    String description;
    int errorCode { 0 };
    int cleanupError { 0 };
};

// Owned by the embedding view. Before destruction it must detach every owned
// download. A download is held strongly by the owner until its terminal event.
// No client callback occurs from a curl or file worker thread.
class SymbianDownloadClient {
public:
    virtual ~SymbianDownloadClient() = default;
    virtual void chooseDownloadDestination(SymbianDownload&, const URL& source, const String& suggestedFilename,
        CompletionHandler<void(String&&)>&&) = 0;
    virtual std::unique_ptr<DownloadFile> createDownloadFile() = 0;
    virtual void downloadProgress(SymbianDownload&, uint64_t received, std::optional<uint64_t> total) = 0;
    virtual void downloadFinished(SymbianDownload&, const String& destination, const DownloadResult&) = 0;
};

class SymbianDownload final : public RefCounted<SymbianDownload>, private WebCore::ResourceHandleClient {
public:
    static Ref<SymbianDownload> create(SymbianDownloadClient&, String suggestedFilename = { });
    ~SymbianDownload();

    // First version supports HTTP(S). Other schemes fail explicitly without
    // issuing a replacement request or exposing a partially written target.
    void start(WebCore::NetworkingContext*, const WebCore::ResourceRequest&, RefPtr<WebCore::SecurityOrigin>&&);
    // Transfer the current response's handle, including POST state; never
    // restart a request just because navigation became an attachment.
    void convert(WebCore::ResourceLoader&, const WebCore::ResourceResponse&);
    void cancel();
    void destinationFailed(int nativeError);
    void detachClient();
    uint64_t receivedBytes() const { return m_received; }
    bool finished() const { return m_state == State::Finished; }

private:
    explicit SymbianDownload(SymbianDownloadClient&, String&&);
    enum class State : uint8_t { Created, AwaitingResponse, ChoosingDestination, Receiving, Finished };
    static constexpr size_t maximumPendingDataBytes = 256 * 1024;

    void receiveResponse(const WebCore::ResourceResponse&);
    void chooseDestination(String&&);
    void reportProgress();
    void progressTimerFired();
    void finish(DownloadResult&&);
    void stopNetwork();
    void willSendRequestAsync(WebCore::ResourceHandle*, WebCore::ResourceRequest&&, WebCore::ResourceResponse&&,
        CompletionHandler<void(WebCore::ResourceRequest&&)>&&) final;
    void didReceiveResponseAsync(WebCore::ResourceHandle*, WebCore::ResourceResponse&&, CompletionHandler<void()>&&) final;
    void didReceiveBuffer(WebCore::ResourceHandle*, const WebCore::FragmentedSharedBuffer&, int) final;
    void didFinishLoading(WebCore::ResourceHandle*, const WebCore::NetworkLoadMetrics&) final;
    void didFail(WebCore::ResourceHandle*, const WebCore::ResourceError&) final;
    void wasBlocked(WebCore::ResourceHandle*) final;
    void cannotShowURL(WebCore::ResourceHandle*) final;
    void didReceiveAuthenticationChallenge(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&) final;

    SymbianDownloadClient* m_client;
    State m_state { State::Created };
    RefPtr<WebCore::ResourceHandle> m_handle;
    CompletionHandler<void()> m_continueResponse;
    std::unique_ptr<DownloadFile> m_file;
    URL m_source;
    String m_suggestedFilename;
    String m_destination;
    uint64_t m_received { 0 };
    std::optional<uint64_t> m_total;
    MonotonicTime m_lastProgress;
    WebCore::Timer m_progressTimer;
};

} // namespace WebKitSymbian
