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

#include "SymbianPlatformStrategies.h"
#include "WebKitSymbianClipboard.h"

#include "WebResourceLoadScheduler.h"
#include <WebCore/BlobRegistry.h>
#include <WebCore/BlobRegistryImpl.h>
#include <WebCore/MediaStrategy.h>
#if ENABLE(WEB_AUDIO)
#include <WebCore/AudioDestination.h>
#endif
#include <WebCore/Pasteboard.h>
#include <WebCore/PasteboardCustomData.h>
#include <WebCore/PasteboardItemInfo.h>
#include <WebCore/PasteboardStrategy.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/PolicyContainer.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringView.h>

namespace WebKitSymbian {

using namespace WebCore;

namespace {

class SymbianMediaStrategy final : public MediaStrategy {
#if ENABLE(WEB_AUDIO)
    Ref<AudioDestination> createAudioDestination(const AudioDestinationCreationOptions& options) final
    {
        return AudioDestination::create(options);
    }
#endif
};

class SymbianBlobRegistry final : public BlobRegistry {
private:
    void registerInternalFileBlobURL(const URL& url, Ref<BlobDataFileReference>&& reference, const String&, const String& contentType) final { m_blobRegistry.registerInternalFileBlobURL(url, WTF::move(reference), contentType); }
    void registerInternalBlobURL(const URL& url, Vector<BlobPart>&& parts, const String& contentType) final { m_blobRegistry.registerInternalBlobURL(url, WTF::move(parts), contentType); }
    void registerBlobURL(const URL& url, const URL& source, const PolicyContainer& policy, const std::optional<SecurityOriginData>& origin) final { m_blobRegistry.registerBlobURL(url, source, policy, origin); }
    void registerInternalBlobURLOptionallyFileBacked(const URL& url, const URL& source, RefPtr<BlobDataFileReference>&& reference, const String& type) final { m_blobRegistry.registerInternalBlobURLOptionallyFileBacked(url, source, WTF::move(reference), type, { }); }
    void registerInternalBlobURLForSlice(const URL& url, const URL& source, long long start, long long end, const String& type) final { m_blobRegistry.registerInternalBlobURLForSlice(url, source, start, end, type); }
    void unregisterBlobURL(const URL& url, const std::optional<SecurityOriginData>& origin) final { m_blobRegistry.unregisterBlobURL(url, origin); }
    void registerBlobURLHandle(const URL& url, const std::optional<SecurityOriginData>& origin) final { m_blobRegistry.registerBlobURLHandle(url, origin); }
    void unregisterBlobURLHandle(const URL& url, const std::optional<SecurityOriginData>& origin) final { m_blobRegistry.unregisterBlobURLHandle(url, origin); }
    String blobType(const URL& url) final { return m_blobRegistry.blobType(url); }
    unsigned long long blobSize(const URL& url) final { return m_blobRegistry.blobSize(url); }
    void writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& urls, CompletionHandler<void(Vector<String>&&)>&& completion) final { m_blobRegistry.writeBlobsToTemporaryFilesForIndexedDB(urls, WTF::move(completion)); }
    BlobRegistryImpl* blobRegistryImpl() final { return &m_blobRegistry; }

    BlobRegistryImpl m_blobRegistry;
};

class SymbianPasteboardStrategy final : public PasteboardStrategy {
    static bool isPlainText(const String& type)
    {
        return type == "text/plain"_s || type == "text/plain;charset=utf-8"_s;
    }

    bool hasText() const
    {
        auto* client = clipboardClient();
        return client && client->hasPlainText();
    }

    bool writeText(const String& text)
    {
        auto* client = clipboardClient();
        if (!client || text.isNull() || text.length() > maximumClipboardTextLength)
            return false;
        auto characters = StringView(text).upconvertedCharacters();
        return client->writePlainText({ reinterpret_cast<const uint16_t*>(characters.get()), static_cast<int>(text.length()) });
    }

    class ReadResult final : public TextResult {
    public:
        void setText(Text text) final
        {
            // The native result is copied while its storage is still alive.
            bool wasCalled = called;
            called = true;
            if (wasCalled || text.length < 0 || text.length > maximumClipboardTextLength || (text.length && !text.characters)) {
                valid = false;
                return;
            }
            valid = true;
            value = text.length ? String(std::span(reinterpret_cast<const UChar*>(text.characters), static_cast<size_t>(text.length))) : emptyString();
        }
        String value;
        bool called { false };
        bool valid { false };
    };

    String readStringFromPasteboard(size_t index, const String& type, const String&, const PasteboardContext*) final
    {
        auto* client = clipboardClient();
        if (index || !isPlainText(type) || !client)
            return { };
        ReadResult result;
        if (!client->readPlainText(result) || !result.called || !result.valid)
            return { };
        return result.value;
    }

    RefPtr<SharedBuffer> readBufferFromPasteboard(std::optional<size_t>, const String&, const String&, const PasteboardContext*) final { return nullptr; }
    URL readURLFromPasteboard(size_t, const String&, String&, const PasteboardContext*) final { return { }; }

    std::optional<PasteboardItemInfo> informationForItemAtIndex(size_t index, const String&, int64_t, const PasteboardContext*) final
    {
        if (index || !hasText())
            return std::nullopt;
        PasteboardItemInfo info;
        info.platformTypesByFidelity = { "text/plain"_s, "text/plain;charset=utf-8"_s };
        return info;
    }

    std::optional<Vector<PasteboardItemInfo>> allPasteboardItemInfo(const String& name, int64_t changeCount, const PasteboardContext* context) final
    {
        Vector<PasteboardItemInfo> items;
        if (auto info = informationForItemAtIndex(0, name, changeCount, context))
            items.append(WTF::move(*info));
        return items;
    }

    int getPasteboardItemsCount(const String&, const PasteboardContext*) final { return hasText() ? 1 : 0; }
    Vector<String> typesSafeForDOMToReadAndWrite(const String&, const String&, const PasteboardContext*) final
    {
        return hasText() ? Vector<String> { "text/plain"_s } : Vector<String> { };
    }
    int64_t writeCustomData(const Vector<PasteboardCustomData>& items, const String&, const PasteboardContext*) final
    {
        if (items.size() != 1)
            return 0;
        // HTML, custom types, files and rich-text fields are not advertised.
        // Do not retain stale process-local content after an external copy.
        const auto& item = items[0];
        for (const auto& type : item.orderedTypes()) {
            if (isPlainText(type))
                return writeText(item.readString(type)) ? 1 : 0;
        }
        return 0;
    }
    bool containsStringSafeForDOMToReadForType(const String& type, const String&, const PasteboardContext*) final { return isPlainText(type) && hasText(); }

    void getTypes(Vector<String>& types) final
    {
        types = hasText() ? Vector<String> { "text/plain"_s, "text/plain;charset=utf-8"_s } : Vector<String> { };
    }
    bool writeToPasteboard(const PasteboardWebContent& content) final { return writeText(content.text); }
    bool writeToPasteboard(const String& type, const String& text) final { return isPlainText(type) && writeText(text); }
    bool clearPasteboard() final
    {
        auto* client = clipboardClient();
        return client && client->clear();
    }
};

} // namespace

void SymbianPlatformStrategies::initialize()
{
    static NeverDestroyed<SymbianPlatformStrategies> platformStrategies;
    setPlatformStrategies(&platformStrategies.get());
}

LoaderStrategy* SymbianPlatformStrategies::createLoaderStrategy()
{
    return new WebResourceLoadScheduler;
}

PasteboardStrategy* SymbianPlatformStrategies::createPasteboardStrategy()
{
    return new SymbianPasteboardStrategy;
}

MediaStrategy* SymbianPlatformStrategies::createMediaStrategy()
{
    return new SymbianMediaStrategy;
}

BlobRegistry* SymbianPlatformStrategies::createBlobRegistry()
{
    return new SymbianBlobRegistry;
}

} // namespace WebKitSymbian
