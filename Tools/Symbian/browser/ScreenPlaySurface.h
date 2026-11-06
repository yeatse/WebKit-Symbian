/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include "NativeHeapDelete.h"
#include <e32base.h>
#include <graphics/surface.h>
#include <graphics/surfacemanager.h>
#include "ScreenPlayUpdateSession.h"
#include "ScreenPlayWindowServer.h"
#include <w32std.h>

class MScreenPlaySurfaceObserver {
public:
    // Request a redraw only; do not delete the presenter in this call.
    virtual void ScreenPlaySurfaceReady() = 0;
};

// GUI-thread software output to an owned opaque ScreenPlay surface.
// Does not own the window/session or change their GC/transparency settings.
// PRECONDITION: the window has never been set transparent. Detach AND delete
// this presenter before enabling window alpha (for example to display MMF).
class CScreenPlaySurface final : public CBase {
public:
    SYMBIAN_BROWSER_NATIVE_DELETE
    struct TFrame {
        TUint8* iPixels { nullptr };
        TInt iStride { 0 };
        TRect iDirty;
        TInt iBuffer { -1 };
        TUint iSerial { 0 };
    };
    // On a leave, aFailedStep (when given) names the step that failed.
    static CScreenPlaySurface* NewL(RWsSession&, RWindow&, TInt aScreen,
        MScreenPlaySurfaceObserver&, const char** aFailedStep = nullptr);
    ~CScreenPlaySurface();
    // None: synchronously paint frame.iDirty, then SubmitFrame.
    // AlreadyExists: current frame valid, no new pixels needed.
    // InUse: both slots in flight; retain invalidation and await Ready.
    // Other errors: detach and fully repaint through the bitmap fallback.
    // Positive dimensions required. All methods are nonleaving after NewL.
    TInt BeginFrame(const TSize&, const TRect& aInvalidated, TFrame&);
    TInt SubmitFrame(const TFrame&);
    void AbortFrame(const TFrame&);
    void Detach();
    TInt LastError() const { return iLastError; }

private:
    class CAvailable;
    CScreenPlaySurface(RWsSession&, RWindow&, TInt, MScreenPlaySurfaceObserver&);
    void ConstructL(const char*& aStep);
    TInt CreateSurface(const TSize&);
    TInt CheckBackground() const;
    TInt Attach();
    void FillPaintBackground(TInt, const TRect&);
    void FlattenPaintedPixels(TInt, const TRect&);
    void AddDamage(const TRect&);
    TBool MatchesFrame(const TFrame&) const;
    void CancelNotifications();
    void ReleaseSurface();
    void BufferAvailable(TInt, TInt);
    TInt Fail(TInt);

    RWsSession& iWs;
    RWindow& iWindow;
    RScreenPlayWindowServer iCalls;
    TBool iCallsOpen { EFalse };
    TInt iScreen;
    MScreenPlaySurfaceObserver& iObserver;
    RSurfaceManager iManager;
    RScreenPlayUpdateSession iUpdates;
    RChunk iChunk;
    TSurfaceId iSurface { TSurfaceId::CreateNullId() };
    CAvailable* iAvailable[2] { nullptr, nullptr };
    TRect iDamage[2];
    TInt iOffsets[2] { 0, 0 };
    TBool iFree[2] { ETrue, ETrue };
    TSize iSize;
    TInt iStride { 0 };
    TInt iPainting { -1 };
    TRect iPaintingDamage;
    TRect iPendingDamage;
    TUint iSerial { 0 };
    TInt iLastError { KErrNone };
    TBool iManagerOpen { EFalse };
    TBool iUpdatesOpen { EFalse };
    TBool iRegistered { EFalse };
    TBool iHaveFrame { EFalse };
    TBool iWaiting { EFalse };
    TBool iStopping { EFalse };
    TBool iCancelling { EFalse };
};
