#include "MediaVolumeKeys.h"

#include "BrowserLog.h"
#include "MediaPlayers.h"

#include <coecntrl.h>
#include <coemain.h>
#include <eikappui.h>
#include <eikenv.h>
#include <gdi.h>
#include <remconcoreapitarget.h>
#include <remconinterfaceselector.h>
#include <w32std.h>

namespace {

const TInt KBarWidth = 220;
const TInt KBarHeight = 44;
const TInt KBarTopMargin = 24;
const TInt KBarShownMicroseconds = 1500000;
const TInt KRepeatDelayMicroseconds = 400000;
const TInt KRepeatIntervalMicroseconds = 150000;

} // namespace

// A small window in front of the page: a speaker mark and one segment for
// each step, filled up to the level. It goes away by itself.
class CMediaVolumeBar final : public CCoeControl {
public:
    SYMBIAN_BROWSER_NATIVE_DELETE

    void ConstructL()
    {
        CreateWindowL();
        // A touch on it hides it; it does not keep the pointer after that.
        Window().SetPointerGrab(EFalse);
        Window().SetNonFading(ETrue);
        MakeVisible(EFalse);
        ActivateL();
        iHide = CPeriodic::NewL(CActive::EPriorityStandard);
    }

    ~CMediaVolumeBar() override { delete iHide; }

    void Show(TInt aLevel)
    {
        iLevel = aLevel;
        TRect client = CEikonEnv::Static()->EikAppUi()->ClientRect();
        TPoint origin(client.iTl.iX + (client.Width() - KBarWidth) / 2, client.iTl.iY + KBarTopMargin);
        SetExtent(origin, TSize(KBarWidth, KBarHeight));
        Window().SetOrdinalPosition(0);
        MakeVisible(ETrue);
        DrawNow();
        iHide->Cancel();
        iHide->Start(KBarShownMicroseconds, KBarShownMicroseconds, TCallBack(Hide, this));
    }

private:
    static TInt Hide(TAny* aSelf)
    {
        auto* self = static_cast<CMediaVolumeBar*>(aSelf);
        self->iHide->Cancel();
        self->MakeVisible(EFalse);
        return KErrNone;
    }

    void HandlePointerEventL(const TPointerEvent& aEvent) override
    {
        if (aEvent.iType == TPointerEvent::EButton1Down)
            Hide(this);
    }

    void Draw(const TRect&) const override
    {
        CWindowGc& gc = SystemGc();
        TRect rect = Rect();
        gc.SetPenStyle(CGraphicsContext::ENullPen);
        gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
        gc.SetBrushColor(TRgb(40, 40, 40));
        gc.DrawRect(rect);

        // The speaker: a box and a cone, then a slash through it when silent.
        const TInt middle = rect.Height() / 2;
        gc.SetBrushColor(KRgbWhite);
        gc.DrawRect(TRect(10, middle - 5, 18, middle + 5));
        TPoint cone[] = { TPoint(18, middle - 5), TPoint(28, middle - 13), TPoint(28, middle + 13), TPoint(18, middle + 5) };
        gc.DrawPolygon(cone, 4);
        if (!iLevel) {
            gc.SetPenStyle(CGraphicsContext::ESolidPen);
            gc.SetPenColor(TRgb(230, 80, 60));
            gc.SetPenSize(TSize(3, 3));
            gc.DrawLine(TPoint(8, middle + 13), TPoint(32, middle - 13));
            gc.SetPenStyle(CGraphicsContext::ENullPen);
        }

        // Segments growing taller to the right.
        const TInt left = 40, right = rect.Width() - 10, gap = 3;
        const TInt width = (right - left - gap * (KMediaVolumeSteps - 1)) / KMediaVolumeSteps;
        const TInt bottom = rect.Height() - 10, tallest = rect.Height() - 20;
        for (TInt i = 0; i < KMediaVolumeSteps; ++i) {
            const TInt height = tallest * (i + 3) / (KMediaVolumeSteps + 2);
            const TInt x = left + i * (width + gap);
            gc.SetBrushColor(i < iLevel ? TRgb(80, 170, 255) : TRgb(90, 90, 90));
            gc.DrawRect(TRect(x, bottom - height, x + width, bottom));
        }
    }

    CPeriodic* iHide { nullptr };
    TInt iLevel { 0 };
};

CMediaVolumeKeys* CMediaVolumeKeys::NewL()
{
    auto* self = new (ELeave) CMediaVolumeKeys;
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
}

void CMediaVolumeKeys::ConstructL()
{
    iBar = new (ELeave) CMediaVolumeBar;
    iBar->ConstructL();
    iRepeat = CPeriodic::NewL(CActive::EPriorityStandard);
    TRAPD(error, {
        iSelector = CRemConInterfaceSelector::NewL();
        iTarget = CRemConCoreApiTarget::NewL(*iSelector, *this);
        iSelector->OpenTargetL();
    });
    if (error) {
        BrowserLog("media: volume keys unavailable (%d)", error);
        delete iSelector; // Deletes iTarget.
        iSelector = nullptr;
        iTarget = nullptr;
        return;
    }
    BrowserLog("media: volume keys on, level %d of %d", MediaVolumeLevel(), KMediaVolumeSteps);
}

CMediaVolumeKeys::~CMediaVolumeKeys()
{
    delete iSelector; // Deletes iTarget.
    delete iRepeat;
    delete iBar;
}

void CMediaVolumeKeys::MrccatoCommand(TRemConCoreApiOperationId aOperationId, TRemConCoreApiButtonAction aButtonAct)
{
    if (aOperationId != ERemConCoreApiVolumeUp && aOperationId != ERemConCoreApiVolumeDown)
        return;
    const TInt delta = aOperationId == ERemConCoreApiVolumeUp ? 1 : -1;
    switch (aButtonAct) {
    case ERemConCoreApiButtonClick:
        Step(delta);
        break;
    case ERemConCoreApiButtonPress:
        Step(delta);
        iRepeatDelta = delta;
        iRepeat->Cancel();
        iRepeat->Start(KRepeatDelayMicroseconds, KRepeatIntervalMicroseconds, TCallBack(Repeat, this));
        break;
    case ERemConCoreApiButtonRelease:
        iRepeat->Cancel();
        break;
    }

    // The remote control server expects an answer to each command.
    TRequestStatus status;
    if (delta > 0)
        iTarget->VolumeUpResponse(status, KErrNone);
    else
        iTarget->VolumeDownResponse(status, KErrNone);
    User::WaitForRequest(status);
}

TBool CMediaVolumeKeys::Step(TInt aDelta)
{
    const TInt previous = MediaVolumeLevel();
    const TInt level = SetMediaVolumeLevel(previous + aDelta);
    iBar->Show(level);
    return level != previous;
}

TInt CMediaVolumeKeys::Repeat(TAny* aSelf)
{
    // Also stops at either end, should the release not come.
    auto* self = static_cast<CMediaVolumeKeys*>(aSelf);
    if (!self->Step(self->iRepeatDelta))
        self->iRepeat->Cancel();
    return KErrNone;
}
