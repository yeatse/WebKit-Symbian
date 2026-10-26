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

#include "WebResourceLoadScheduler.h"
#include <WebCore/BlobRegistry.h>
#include <WebCore/BlobRegistryImpl.h>
#include <WebCore/MediaStrategy.h>
#include <WebCore/Pasteboard.h>
#include <WebCore/PasteboardItemInfo.h>
#include <WebCore/PasteboardStrategy.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringHash.h>

namespace WebKitSymbian {

using namespace WebCore;

namespace {

class SymbianMediaStrategy final : public MediaStrategy {
};

class SymbianBlobRegistry final : public BlobRegistry {
private:
    void registerFileBlobURL(const URL& url, Ref<BlobDataFileReference>&& reference, const String&, const String& contentType) final { m_blobRegistry.registerFileBlobURL(url, WTFMove(reference), contentType); }
    void registerBlobURL(const URL& url, Vector<BlobPart>&& parts, const String& contentType) final { m_blobRegistry.registerBlobURL(url, WTFMove(parts), contentType); }
    void registerBlobURL(const URL& url, const URL& srcURL) final { m_blobRegistry.registerBlobURL(url, srcURL); }
    void registerBlobURLOptionallyFileBacked(const URL& url, const URL& srcURL, RefPtr<BlobDataFileReference>&& reference, const String& contentType) final { m_blobRegistry.registerBlobURLOptionallyFileBacked(url, srcURL, WTFMove(reference), contentType); }
    void registerBlobURLForSlice(const URL& url, const URL& srcURL, long long start, long long end) final { m_blobRegistry.registerBlobURLForSlice(url, srcURL, start, end); }
    void unregisterBlobURL(const URL& url) final { m_blobRegistry.unregisterBlobURL(url); }
    unsigned long long blobSize(const URL& url) final { return m_blobRegistry.blobSize(url); }
    void writeBlobsToTemporaryFiles(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&&)>&& completionHandler) final { m_blobRegistry.writeBlobsToTemporaryFiles(blobURLs, WTFMove(completionHandler)); }
    BlobRegistryImpl* blobRegistryImpl() final { return &m_blobRegistry; }

    BlobRegistryImpl m_blobRegistry;
};

// The pasteboard lives in the process, by MIME type.
// FIXME: Exchange text with the system clipboard (CClipboard).
class SymbianPasteboardStrategy final : public PasteboardStrategy {
private:
    String readStringFromPasteboard(size_t index, const String& type, const String&, const PasteboardContext*) final
    {
        return index ? String() : m_items.get(type);
    }

    RefPtr<SharedBuffer> readBufferFromPasteboard(size_t, const String&, const String&, const PasteboardContext*) final { return nullptr; }
    URL readURLFromPasteboard(size_t, const String&, String&, const PasteboardContext*) final { return { }; }

    Optional<PasteboardItemInfo> informationForItemAtIndex(size_t index, const String&, int64_t, const PasteboardContext*) final
    {
        if (index || m_items.isEmpty())
            return WTF::nullopt;
        PasteboardItemInfo info;
        info.platformTypesByFidelity = copyToVector(m_items.keys());
        return info;
    }

    Optional<Vector<PasteboardItemInfo>> allPasteboardItemInfo(const String& name, int64_t changeCount, const PasteboardContext* context) final
    {
        Vector<PasteboardItemInfo> items;
        if (auto info = informationForItemAtIndex(0, name, changeCount, context))
            items.append(WTFMove(*info));
        return items;
    }

    int getPasteboardItemsCount(const String&, const PasteboardContext*) final { return m_items.isEmpty() ? 0 : 1; }
    Vector<String> typesSafeForDOMToReadAndWrite(const String&, const String&, const PasteboardContext*) final { return copyToVector(m_items.keys()); }
    int64_t writeCustomData(const Vector<PasteboardCustomData>&, const String&, const PasteboardContext*) final { return 0; }
    bool containsStringSafeForDOMToReadForType(const String& type, const String&, const PasteboardContext*) final { return m_items.contains(type); }

    void getTypes(Vector<String>& types) final { types = copyToVector(m_items.keys()); }

    void writeToPasteboard(const PasteboardWebContent& content) final
    {
        m_items.clear();
        m_items.set("text/plain;charset=utf-8"_s, content.text);
        m_items.set("text/html;charset=utf-8"_s, content.markup);
    }

    void writeToPasteboard(const String& type, const String& text) final
    {
        m_items.clear();
        m_items.set(type, text);
    }

    HashMap<String, String> m_items;
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
