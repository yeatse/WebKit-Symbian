/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Compile this native boundary separately, with Symbian native leaves enabled.
#include "JPEGSystemDecoderSymbianNative.h"

#include <e32base.h>
#include <f32file.h>
#include <fbs.h>
#include <imageconversion.h>

namespace WebCore::SymbianJPEG {
namespace {

_LIT8(KJPEGMimeType, "image/jpeg");

// Completes a nested scheduler loop when ICL completes the conversion.
class ConversionWaiter final : public CActive {
public:
    explicit ConversionWaiter(CActiveSchedulerWait& wait)
        : CActive(EPriorityStandard)
        , m_wait(wait)
    {
        CActiveScheduler::Add(this);
    }
    ~ConversionWaiter() override { Cancel(); }

    TRequestStatus& status() { return iStatus; }
    void start() { SetActive(); }

private:
    void RunL() override
    {
        if (m_wait.IsStarted())
            m_wait.AsyncStop();
    }
    void DoCancel() override { }

    CActiveSchedulerWait& m_wait;
};

struct Session {
    CTrapCleanup* cleanup { nullptr };
    CActiveScheduler* scheduler { nullptr };
    RFs fs;
    bool fsOpen { false };
    bool fbsOpen { false };
    CFbsBitmap* bitmap { nullptr };
};

void destroySession(Session* session)
{
    delete session->bitmap;
    if (session->fbsOpen)
        RFbsSession::Disconnect();
    if (session->fsOpen)
        session->fs.Close();
    if (session->scheduler) {
        CActiveScheduler::Install(nullptr);
        delete session->scheduler;
    }
    delete session->cleanup;
    delete session;
}

void decodeL(Session& session, const TDesC8& source, const TSize& size)
{
    // Exif data is not needed: WebKit reads the orientation itself.
    CImageDecoder* decoder = CImageDecoder::DataNewL(session.fs, source, KJPEGMimeType,
        static_cast<CImageDecoder::TOptions>(CImageDecoder::EOptionNoDither | CImageDecoder::EOptionIgnoreExifMetaData));
    CleanupStack::PushL(decoder);
    if (decoder->FrameCount() < 1 || decoder->FrameInfo(0).iOverallSizeInPixels != size)
        User::Leave(KErrCorrupt);

    CFbsBitmap* bitmap = new (ELeave) CFbsBitmap;
    CleanupStack::PushL(bitmap);
    User::LeaveIfError(bitmap->Create(size, EColor16MU));

    CActiveSchedulerWait wait;
    ConversionWaiter waiter(wait);
    decoder->Convert(&waiter.status(), *bitmap);
    waiter.start();
    wait.Start();
    User::LeaveIfError(waiter.status().Int());

    CleanupStack::Pop(bitmap);
    CleanupStack::PopAndDestroy(decoder);
    session.bitmap = bitmap;
}

} // namespace

void* createSession(int& error)
{
    Session* session = new (std::nothrow) Session;
    if (!session) {
        error = KErrNoMemory;
        return nullptr;
    }
    if (!User::TrapHandler()) {
        session->cleanup = CTrapCleanup::New();
        if (!session->cleanup) {
            delete session;
            error = KErrNoMemory;
            return nullptr;
        }
    }
    error = KErrNone;
    if (!CActiveScheduler::Current()) {
        // CBase's operator new takes User's heap, which its library-side
        // deleting destructor frees; std::nothrow would take the global heap.
        TRAP(error, session->scheduler = new (ELeave) CActiveScheduler);
        if (!error)
            CActiveScheduler::Install(session->scheduler);
    }
    if (!error) {
        error = session->fs.Connect();
        session->fsOpen = !error;
    }
    if (!error) {
        error = RFbsSession::Connect();
        session->fbsOpen = !error;
    }
    if (error) {
        destroySession(session);
        return nullptr;
    }
    return session;
}

int decode(void* opaque, const uint8_t* data, size_t size, int width, int height)
{
    auto& session = *static_cast<Session*>(opaque);
    discardBitmap(opaque);
    if (!data || !size || size > KMaxTInt || width <= 0 || height <= 0)
        return KErrArgument;
    TPtrC8 source(data, static_cast<TInt>(size));
    TRAPD(error, decodeL(session, source, TSize(width, height)));
    return error;
}

void copyPixels(void* opaque, uint32_t* pixels, size_t stride)
{
    auto& session = *static_cast<Session*>(opaque);
    CFbsBitmap* bitmap = session.bitmap;
    if (!bitmap)
        return;
    TSize size = bitmap->SizeInPixels();
    TInt sourceStride = CFbsBitmap::ScanLineLength(size.iWidth, EColor16MU) / 4;
    bitmap->LockHeap();
    const TUint32* source = bitmap->DataAddress();
    for (TInt y = 0; y < size.iHeight; ++y) {
        const TUint32* sourceRow = source + y * sourceStride;
        uint32_t* row = pixels + y * stride;
        // EColor16MU leaves the top byte undefined; JPEGs are opaque.
        for (TInt x = 0; x < size.iWidth; ++x)
            row[x] = sourceRow[x] | 0xff000000;
    }
    bitmap->UnlockHeap();
    discardBitmap(opaque);
}

void discardBitmap(void* opaque)
{
    auto& session = *static_cast<Session*>(opaque);
    delete session.bitmap;
    session.bitmap = nullptr;
}

} // namespace WebCore::SymbianJPEG
