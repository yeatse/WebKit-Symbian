/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "SymbianDownloadManager.h"

#include "WebKitSymbian.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/MainThread.h>
#include <wtf/Vector.h>

namespace WebKitSymbian {

namespace {

class DownloadText {
public:
    explicit DownloadText(const String& value)
    {
        m_characters.reserveInitialCapacity(value.length());
        for (unsigned i = 0; i < value.length(); ++i)
            m_characters.append(value[i]);
    }
    Text text() const { return { m_characters.span().data(), static_cast<int>(m_characters.size()) }; }

private:
    Vector<uint16_t> m_characters;
};

class DownloadPathResult final : public TextResult {
public:
    void setText(Text text) final
    {
        m_path = { };
        if (!text.characters || text.length <= 0 || text.length > 256)
            return;
        char16_t buffer[256];
        for (int i = 0; i < text.length; ++i)
            buffer[i] = text.characters[i];
        m_path = String(std::span { buffer, static_cast<size_t>(text.length) });
    }
    String takePath() { return std::exchange(m_path, { }); }

private:
    String m_path;
};

} // namespace

Ref<SymbianDownloadManager> SymbianDownloadManager::create(WebViewClient& client)
{
    return adoptRef(*new SymbianDownloadManager(client));
}

SymbianDownloadManager::SymbianDownloadManager(WebViewClient& client)
    : m_client(&client)
{
}

SymbianDownloadManager::~SymbianDownloadManager()
{
    detach();
}

RefPtr<SymbianDownload> SymbianDownloadManager::begin(const String& suggestedFilename)
{
    ASSERT(isMainThread());
    if (!m_client)
        return nullptr;
    auto identifier = ++m_nextIdentifier;
    RELEASE_ASSERT(identifier);
    if (m_active) {
        reportFinished(identifier, { }, { DownloadFailure::Busy, "Another download is already active in this view"_s });
        return nullptr;
    }
    m_identifier = identifier;
    RefPtr download = SymbianDownload::create(*this, suggestedFilename);
    m_active = download;
    m_client->didStartDownload(identifier);
    return m_client && m_active == download && !download->finished() ? download : nullptr;
}

void SymbianDownloadManager::start(WebCore::NetworkingContext* context, const WebCore::ResourceRequest& request,
    RefPtr<WebCore::SecurityOrigin>&& origin, const String& suggestedFilename)
{
    Ref protectedThis { *this };
    if (auto download = begin(suggestedFilename))
        download->start(context, request, WTF::move(origin));
}

void SymbianDownloadManager::convert(WebCore::ResourceLoader* loader, const WebCore::ResourceResponse& response)
{
    Ref protectedThis { *this };
    if (!loader) {
        auto identifier = ++m_nextIdentifier;
        RELEASE_ASSERT(identifier);
        reportFinished(identifier, { }, { DownloadFailure::Unsupported, "No live network response is available; the request was not repeated"_s });
        return;
    }
    if (auto download = begin({ }))
        download->convert(*loader, response);
}

void SymbianDownloadManager::chooseDownloadDestination(SymbianDownload& download, const URL& source,
    const String& suggestedFilename, CompletionHandler<void(String&&)>&& continuation)
{
    Ref protectedThis { *this };
    if (!m_client || m_active.get() != &download) {
        continuation({ });
        return;
    }
    DownloadText sourceText(source.string());
    DownloadText filenameText(suggestedFilename);
    DownloadPathResult result;
    int selection = m_client->chooseDownloadDestination(m_identifier, sourceText.text(), filenameText.text(), result);
    // The modal loop may have cancelled the transfer, detached the client or
    // closed the view. Do not call back into that embedding object afterward.
    if (!m_client || m_active.get() != &download || download.finished())
        continuation({ });
    else {
        if (selection < 0)
            download.destinationFailed(selection);
        continuation(!selection ? result.takePath() : String { });
    }
}

std::unique_ptr<DownloadFile> SymbianDownloadManager::createDownloadFile()
{
    Ref protectedThis { *this };
    return m_client ? std::unique_ptr<DownloadFile>(m_client->createDownloadFile()) : nullptr;
}

void SymbianDownloadManager::downloadProgress(SymbianDownload& download, uint64_t received, std::optional<uint64_t> total)
{
    Ref protectedThis { *this };
    if (m_client && m_active.get() == &download)
        m_client->didChangeDownloadProgress(m_identifier, received, total ? static_cast<int64_t>(*total) : -1);
}

void SymbianDownloadManager::downloadFinished(SymbianDownload& download, const String& path, const DownloadResult& result)
{
    Ref protectedThis { *this };
    if (m_active.get() != &download)
        return;
    auto identifier = std::exchange(m_identifier, 0);
    m_active = nullptr;
    reportFinished(identifier, path, result);
}

void SymbianDownloadManager::reportFinished(uint64_t identifier, const String& path, const DownloadResult& result)
{
    if (!m_client)
        return;
    DownloadText pathText(path);
    DownloadText descriptionText(result.description);
    m_client->didFinishDownload(identifier, pathText.text(), result.failure, result.errorCode, result.cleanupError, descriptionText.text());
}

void SymbianDownloadManager::cancel(uint64_t identifier)
{
    Ref protectedThis { *this };
    if (identifier == m_identifier && m_active)
        m_active->cancel();
}

void SymbianDownloadManager::detach()
{
    ASSERT(isMainThread());
    m_client = nullptr;
    m_identifier = 0;
    if (auto download = std::exchange(m_active, nullptr))
        download->detachClient();
}

} // namespace WebKitSymbian
