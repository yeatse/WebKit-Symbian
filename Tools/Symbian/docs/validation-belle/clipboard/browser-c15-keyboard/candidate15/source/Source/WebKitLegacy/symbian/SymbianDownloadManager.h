/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "SymbianDownload.h"

namespace WebKitSymbian {

class WebViewClient;

// Independent of the WebView lifetime: a native save dialog may dispatch a
// nested event loop. detach() closes the borrowed embedding client before the
// view goes away; in-flight callbacks hold the manager until they return.
class SymbianDownloadManager final : public RefCounted<SymbianDownloadManager>, private SymbianDownloadClient {
public:
    static Ref<SymbianDownloadManager> create(WebViewClient&);
    ~SymbianDownloadManager();

    void start(WebCore::NetworkingContext*, const WebCore::ResourceRequest&, RefPtr<WebCore::SecurityOrigin>&&, const String& suggestedFilename);
    void convert(WebCore::ResourceLoader*, const WebCore::ResourceResponse&);
    bool hasActiveDownload() const { return !!m_active; }
    void cancel(uint64_t identifier);
    void detach();

private:
    explicit SymbianDownloadManager(WebViewClient&);
    RefPtr<SymbianDownload> begin(const String& suggestedFilename);
    void chooseDownloadDestination(SymbianDownload&, const URL&, const String&, CompletionHandler<void(String&&)>&&) final;
    std::unique_ptr<DownloadFile> createDownloadFile() final;
    void downloadProgress(SymbianDownload&, uint64_t, std::optional<uint64_t>) final;
    void downloadFinished(SymbianDownload&, const String&, const DownloadResult&) final;
    void reportFinished(uint64_t, const String&, const DownloadResult&);

    WebViewClient* m_client;
    RefPtr<SymbianDownload> m_active;
    uint64_t m_identifier { 0 };
    uint64_t m_nextIdentifier { 0 };
};

} // namespace WebKitSymbian
