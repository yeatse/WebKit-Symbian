/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "ScreenPlaySurface.h"
#include <graphics/surfaceconfiguration.h>
#include <pixelformats.h>

namespace {
void Accumulate(TRect& aDestination, const TRect& aRect)
{
    if (aRect.IsEmpty())
        return;
    if (aDestination.IsEmpty())
        aDestination = aRect;
    else
        aDestination.BoundingRect(aRect);
}
} // namespace

class CScreenPlaySurface::CAvailable final : public CActive {
public:
    SYMBIAN_BROWSER_NATIVE_DELETE
    CAvailable(CScreenPlaySurface& aOwner, TInt aBuffer)
        : CActive(CActive::EPriorityStandard), iOwner(aOwner), iBuffer(aBuffer)
    {
        CActiveScheduler::Add(this);
    }
    ~CAvailable() { Cancel(); }
    void Arm()
    {
        iStatus = KRequestPending;
        iArmed = ETrue;
        // Notify caches this pointer until Submit. Activate its owner first.
        iOwner.iUpdates.NotifyWhenAvailable(iStatus);
        SetActive();
    }
    void Stop()
    {
        const TBool wasArmed = iArmed;
        Cancel();
        // Only teardown waits, after CancelAllUpdateNotifications.
        if (wasArmed && iStatus == KRequestPending)
            User::WaitForRequest(iStatus);
        iArmed = EFalse;
    }
private:
    void RunL() final
    {
        iArmed = EFalse;
        iOwner.BufferAvailable(iBuffer, iStatus.Int());
    }
    void DoCancel() final { iOwner.CancelNotifications(); }
    CScreenPlaySurface& iOwner;
    TInt iBuffer;
    TBool iArmed { EFalse };
};

CScreenPlaySurface* CScreenPlaySurface::NewL(RWsSession& aWs, RWindow& aWindow,
    TInt aScreen, MScreenPlaySurfaceObserver& aObserver, const char** aFailedStep)
{
    const char* unused = nullptr;
    const char*& step = aFailedStep ? *aFailedStep : unused;
    step = "allocate";
    CScreenPlaySurface* self = new (ELeave) CScreenPlaySurface(aWs, aWindow,
        aScreen, aObserver);
    CleanupStack::PushL(self);
    self->ConstructL(step);
    CleanupStack::Pop(self);
    return self;
}

CScreenPlaySurface::CScreenPlaySurface(RWsSession& aWs, RWindow& aWindow,
    TInt aScreen, MScreenPlaySurfaceObserver& aObserver)
    : iWs(aWs), iWindow(aWindow), iScreen(aScreen), iObserver(aObserver)
{
}

void CScreenPlaySurface::ConstructL(const char*& aStep)
{
    aStep = "window server calls";
    User::LeaveIfError(iCalls.Open());
    iCallsOpen = ETrue;
    aStep = "surface manager";
    User::LeaveIfError(iManager.Open());
    iManagerOpen = ETrue;
    aStep = "surface update server";
    User::LeaveIfError(iUpdates.Connect());
    iUpdatesOpen = ETrue;
    aStep = "allocate";
    iAvailable[0] = new (ELeave) CAvailable(*this, 0);
    iAvailable[1] = new (ELeave) CAvailable(*this, 1);
}

CScreenPlaySurface::~CScreenPlaySurface()
{
    iStopping = ETrue;
    ReleaseSurface();
    delete iAvailable[0];
    delete iAvailable[1];
    if (iUpdatesOpen)
        iUpdates.Close();
    if (iManagerOpen)
        iManager.Close();
    if (iCallsOpen)
        iCalls.Close();
}

TInt CScreenPlaySurface::Fail(TInt aError)
{
    if (aError != KErrNone && iLastError == KErrNone)
        iLastError = aError;
    return aError;
}

TInt CScreenPlaySurface::CreateSurface(const TSize& aSize)
{
    if (aSize.iWidth <= 0 || aSize.iHeight <= 0
        || aSize.iWidth > KMaxTInt / 4)
        return KErrArgument;
    const TInt64 rowBytes = static_cast<TInt64>(aSize.iWidth) * 4;
    const TInt64 minimumBytes = rowBytes * aSize.iHeight;
    if (minimumBytes > KMaxTInt / 2)
        return KErrArgument;
    RSurfaceManager::TSurfaceCreationAttributesBuf request;
    request().iSize = aSize;
    request().iBuffers = 2;
    request().iPixelFormat = EUidPixelFormatARGB_8888_PRE;
    request().iStride = static_cast<TInt>(rowBytes);
    request().iAlignment = 4;
    request().iCacheAttrib = RSurfaceManager::ECached;
    request().iContiguous = EFalse;
    request().iMappable = ETrue;
    TInt error = iManager.CreateSurface(request, iSurface);
    if (error != KErrNone)
        return error;
    RSurfaceManager::TInfoBuf info;
    error = iManager.SurfaceInfo(iSurface, info);
    if (error != KErrNone)
        return error;
    if (info().iSize != aSize || info().iBuffers != 2
        || info().iPixelFormat != EUidPixelFormatARGB_8888_PRE
        || !info().iMappable || info().iStride < rowBytes
        || info().iStride <= 0 || (info().iStride & 3))
        return KErrNotSupported;
    const TInt64 bytes = static_cast<TInt64>(info().iStride) * aSize.iHeight;
    if (bytes <= 0 || bytes > KMaxTInt)
        return KErrCorrupt;
    error = iManager.MapSurface(iSurface, iChunk);
    if (error != KErrNone)
        return error;
    if (!iChunk.Base() || iChunk.Size() <= 0)
        return KErrCorrupt;
    for (TInt buffer = 0; buffer < 2; ++buffer) {
        error = iManager.GetBufferOffset(iSurface, buffer, iOffsets[buffer]);
        if (error != KErrNone)
            return error;
        if (iOffsets[buffer] < 0 || (iOffsets[buffer] & 3)
            || static_cast<TInt64>(iOffsets[buffer]) + bytes > iChunk.Size())
            return KErrCorrupt;
    }
    const TInt64 firstEnd = static_cast<TInt64>(iOffsets[0]) + bytes;
    const TInt64 secondEnd = static_cast<TInt64>(iOffsets[1]) + bytes;
    if (!(firstEnd <= iOffsets[1] || secondEnd <= iOffsets[0]))
        return KErrCorrupt;
    for (TInt buffer = 0; buffer < 2; ++buffer)
        Mem::Fill(iChunk.Base() + iOffsets[buffer], static_cast<TInt>(bytes), 0xff);
    error = iCalls.RegisterSurface(iWs, iScreen, iSurface);
    if (error != KErrNone)
        return error;
    iRegistered = ETrue;
    iSize = aSize;
    iStride = info().iStride;
    iFree[0] = iFree[1] = ETrue;
    iDamage[0] = iDamage[1] = TRect(aSize);
    iPendingDamage = TRect(aSize);
    return KErrNone;
}

void CScreenPlaySurface::FillPaintBackground(TInt aBuffer, const TRect& aRect)
{
    // Each slot retains its own last frame. Repaint its accumulated damage
    // over the same opaque white page base used by the native compositor.
    for (TInt y = aRect.iTl.iY; y < aRect.iBr.iY; ++y) {
        TUint32* row = reinterpret_cast<TUint32*>(iChunk.Base()
            + iOffsets[aBuffer] + y * iStride);
        for (TInt x = aRect.iTl.iX; x < aRect.iBr.iX; ++x)
            row[x] = 0xffffffffu;
    }
}

void CScreenPlaySurface::FlattenPaintedPixels(TInt aBuffer, const TRect& aRect)
{
    // SetBackgroundSurface(config) requires an opaque surface. The engine
    // paints premultiplied ARGB; flatten any explicit transparent operations
    // over white in place. This is not a bitmap-to-surface frame copy.
    for (TInt y = aRect.iTl.iY; y < aRect.iBr.iY; ++y) {
        TUint32* row = reinterpret_cast<TUint32*>(iChunk.Base()
            + iOffsets[aBuffer] + y * iStride);
        for (TInt x = aRect.iTl.iX; x < aRect.iBr.iX; ++x) {
            TUint32 pixel = row[x];
            TUint32 missing = 255u - (pixel >> 24);
            if (!missing)
                continue;
            TUint32 red = ((pixel >> 16) & 255u) + missing;
            TUint32 green = ((pixel >> 8) & 255u) + missing;
            TUint32 blue = (pixel & 255u) + missing;
            if (red > 255u) red = 255u;
            if (green > 255u) green = 255u;
            if (blue > 255u) blue = 255u;
            row[x] = 0xff000000u | (red << 16) | (green << 8) | blue;
        }
    }
}

void CScreenPlaySurface::AddDamage(const TRect& aDamage)
{
    TRect rect(aDamage);
    rect.Intersection(TRect(iSize));
    Accumulate(iDamage[0], rect);
    Accumulate(iDamage[1], rect);
    Accumulate(iPendingDamage, rect);
}

TInt CScreenPlaySurface::CheckBackground() const
{
    // The caller must keep this window opaque while the presenter exists.
    // GetBackgroundSurface is not legal on a transparent RWindow.
    TSurfaceConfiguration configuration;
    TInt error = iCalls.GetBackgroundSurface(iWindow, configuration);
    if (error != KErrNone)
        return error;
    TSurfaceId current = TSurfaceId::CreateNullId();
    configuration.GetSurfaceId(current);
    if (current.IsNull())
        return KErrNotFound;
    if (current != iSurface)
        return KErrAccessDenied;
    return KErrNone;
}

TInt CScreenPlaySurface::BeginFrame(const TSize& aSize, const TRect& aInvalidated,
    TFrame& aFrame)
{
    aFrame = TFrame();
    if (iStopping)
        return KErrNotReady;
    if (iLastError != KErrNone)
        return iLastError;
    if (iPainting >= 0)
        return KErrInUse;
    if (iSurface.IsNull() || iSize != aSize) {
        ReleaseSurface();
        if (iLastError != KErrNone)
            return iLastError;
        TInt error = CreateSurface(aSize);
        if (error != KErrNone) {
            ReleaseSurface();
            return Fail(error);
        }
    }
    AddDamage(aInvalidated);
    if (iHaveFrame) {
        TInt error = CheckBackground();
        if (error == KErrNotFound) {
            iHaveFrame = EFalse;
            AddDamage(TRect(iSize));
        } else if (error != KErrNone)
            return Fail(error);
    }
    if (iHaveFrame && iPendingDamage.IsEmpty())
        return KErrAlreadyExists;
    for (TInt buffer = 0; buffer < 2; ++buffer) {
        if (!iFree[buffer])
            continue;
        iPainting = buffer;
        iPaintingDamage = iDamage[buffer];
        // Snapshot before paint so recursive invalidations survive submission.
        iDamage[buffer] = TRect();
        iPendingDamage = TRect();
        FillPaintBackground(buffer, iPaintingDamage);
        if (!++iSerial)
            ++iSerial;
        aFrame.iBuffer = buffer;
        aFrame.iPixels = iChunk.Base() + iOffsets[buffer];
        aFrame.iStride = iStride;
        aFrame.iDirty = iPaintingDamage;
        aFrame.iSerial = iSerial;
        return KErrNone;
    }
    iWaiting = ETrue;
    return KErrInUse;
}

TBool CScreenPlaySurface::MatchesFrame(const TFrame& aFrame) const
{
    return iPainting >= 0 && iPainting < 2 && !iSurface.IsNull()
        && aFrame.iBuffer == iPainting && aFrame.iSerial == iSerial
        && aFrame.iPixels == iChunk.Base() + iOffsets[iPainting]
        && aFrame.iStride == iStride && aFrame.iDirty == iPaintingDamage;
}

void CScreenPlaySurface::AbortFrame(const TFrame& aFrame)
{
    if (!MatchesFrame(aFrame))
        return;
    AddDamage(iPaintingDamage);
    iPainting = -1;
    iPaintingDamage = TRect();
}

TInt CScreenPlaySurface::Attach()
{
    TInt error = CheckBackground();
    if (error == KErrNone)
        return KErrNone;
    if (error != KErrNotFound)
        return error;
    TSurfaceConfiguration configuration;
    error = configuration.SetSurfaceId(iSurface);
    if (error == KErrNone)
        error = configuration.SetViewport(TRect(iSize));
    if (error == KErrNone)
        error = configuration.SetExtent(TRect(iSize));
    if (error == KErrNone)
        error = iCalls.SetBackgroundSurface(iWindow, configuration, EFalse);
    if (error == KErrNone)
        error = iWs.Finish();
    return error;
}

TInt CScreenPlaySurface::SubmitFrame(const TFrame& aFrame, TUint32* aMarks)
{
    if (!MatchesFrame(aFrame))
        return Fail(KErrArgument);
    const TInt buffer = iPainting;
    FlattenPaintedPixels(buffer, iPaintingDamage);
    if (aMarks)
        aMarks[0] = User::FastCounter();
    TInt error = iManager.SynchronizeCache(iSurface, buffer,
        RSurfaceManager::ESyncBeforeNonCPURead);
    if (aMarks)
        aMarks[1] = User::FastCounter();
    if (error == KErrNone)
        error = Attach();
    if (aMarks)
        aMarks[2] = User::FastCounter();
    if (error != KErrNone) {
        AbortFrame(aFrame);
        return Fail(error);
    }
    iFree[buffer] = EFalse;
    iAvailable[buffer]->Arm();
    // The supported public SUS contract is KAllScreens; never mix modes.
    error = iUpdates.SubmitUpdate(KAllScreens, iSurface, buffer);
    if (error != KErrNone) {
        // Failure may still have handed out an asynchronous status.
        CancelNotifications();
        AbortFrame(aFrame);
        return Fail(error);
    }
    iPainting = -1;
    iPaintingDamage = TRect();
    iHaveFrame = ETrue;
    return KErrNone;
}

void CScreenPlaySurface::BufferAvailable(TInt aBuffer, TInt aError)
{
    if (iStopping || aBuffer < 0 || aBuffer >= 2)
        return;
    if (aError != KErrNone)
        Fail(aError);
    else
        iFree[aBuffer] = ETrue;
    if (iWaiting || aError != KErrNone) {
        iWaiting = EFalse;
        iObserver.ScreenPlaySurfaceReady();
    }
}

void CScreenPlaySurface::CancelNotifications()
{
    if (iCancelling || !iUpdatesOpen)
        return;
    iCancelling = ETrue;
    iUpdates.CancelAllUpdateNotifications();
    for (TInt buffer = 0; buffer < 2; ++buffer) {
        if (iAvailable[buffer])
            iAvailable[buffer]->Stop();
    }
    iCancelling = EFalse;
    // Cancellation releases status objects, not compositor use of pixels.
    // Do not mark slots free; this complete surface is then retired.
}

void CScreenPlaySurface::ReleaseSurface()
{
    if (!iSurface.IsNull()) {
        // MMF/EGL may have replaced our attachment since the previous frame.
        TInt error = CheckBackground();
        if (error == KErrNone) {
            iCalls.RemoveBackgroundSurface(iWindow, EFalse);
            Fail(iWs.Finish());
        } else if (error != KErrNotFound && error != KErrAccessDenied)
            Fail(error);
        // Stop submitting to this window first. Cancellation alone does not
        // remove composition's reference to the previously active buffer.
        CancelNotifications();
        if (iRegistered) {
            iCalls.UnregisterSurface(iWs, iScreen, iSurface);
            Fail(iWs.Finish());
        }
        iChunk.Close();
        Fail(iManager.CloseSurface(iSurface));
    } else {
        CancelNotifications();
        iChunk.Close();
    }
    iSurface = TSurfaceId::CreateNullId();
    iRegistered = EFalse;
    iHaveFrame = EFalse;
    iWaiting = EFalse;
    iPainting = -1;
    iPaintingDamage = iPendingDamage = TRect();
    iDamage[0] = iDamage[1] = TRect();
    iOffsets[0] = iOffsets[1] = 0;
    iSize = TSize();
    iStride = 0;
}

void CScreenPlaySurface::Detach()
{
    TBool wasStopping = iStopping;
    iStopping = ETrue;
    ReleaseSurface();
    iStopping = wasStopping;
}
