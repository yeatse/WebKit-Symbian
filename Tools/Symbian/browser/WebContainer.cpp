#include "WebContainer.h"

#include "NativeDeclarations.h"
#include "AuthenticationUI.h"
#include "BrowserLog.h"
#include "DownloadFileNative.h"
#include "DownloadUI.h"
#include "browser_ui.rsg"
#if SYMBIAN_BROWSER_MEDIA
#include "MediaPlayers.h"
#endif

#include <aknedsts.h>
#include <AknCommonDialogsDynMem.h>
#include <AknQueryDialog.h>
#include <aknedstsobs.h>
#include <coemain.h>
#include <e32debug.h>
#include <e32keys.h>
#include <eikenv.h>
#include <fbs.h>
#include <f32file.h>
#include <hal.h>
#include <hal_data.h>
#include <txtfrmat.h>

using namespace WebKitSymbian;

namespace {

// Windows virtual key codes, which WebCore's key events use.
enum {
    VK_BACK = 0x08, VK_TAB = 0x09, VK_RETURN = 0x0D, VK_ESCAPE = 0x1B, VK_SPACE = 0x20,
    VK_PRIOR = 0x21, VK_NEXT = 0x22, VK_END = 0x23, VK_HOME = 0x24,
    VK_LEFT = 0x25, VK_UP = 0x26, VK_RIGHT = 0x27, VK_DOWN = 0x28, VK_DELETE = 0x2E,
};

// A drag of this many pixels is a scroll, not a tap.
const TInt KTapSlop = 8;

// Editor flags for the AVKON FEP of Symbian^3 and later: the partial
// screen (split view) input, and an editor that shows its selection.
const TInt KEditorFlags = EAknEditorFlagEnablePartialScreen | EAknEditorFlagSelectionVisible;

TPtrC ToDesC(Text aText)
{
    return TPtrC(reinterpret_cast<const TText*>(aText.characters), aText.length);
}

Text ToText(const TDesC& aDes)
{
    return Text { reinterpret_cast<const uint16_t*>(aDes.Ptr()), aDes.Length() };
}

unsigned KeyModifiers(TUint nativeModifiers)
{
    unsigned modifiers = 0;
    if (nativeModifiers & (EModifierShift | EModifierLeftShift | EModifierRightShift))
        modifiers |= ShiftKey;
    if (nativeModifiers & (EModifierCtrl | EModifierLeftCtrl | EModifierRightCtrl))
        modifiers |= ControlKey;
    if (nativeModifiers & (EModifierAlt | EModifierLeftAlt | EModifierRightAlt))
        modifiers |= AltKey;
    return modifiers;
}

TUint KeyCodeForEvent(TUint code, TInt scanCode, TUint nativeModifiers)
{
    // Keep WSERV/FEP's translated code. Consult the physical scan only to
    // distinguish Ctrl+H/I/M from Backspace/Tab/Enter, which share C0 codes.
    // Standard letter scans are ASCII A-Z; do not reinterpret other keys or
    // FEP events without a physical letter scan as shortcut letters. Ctrl+Alt
    // may represent AltGr: its translated character must not be reconstructed.
    unsigned modifiers = KeyModifiers(nativeModifiers);
    if (!(modifiers & ControlKey) || (modifiers & AltKey) || code < 1 || code > 26
        || scanCode < 'A' || scanCode > 'Z')
        return code;
    bool shift = modifiers & ShiftKey;
    bool capsLock = nativeModifiers & EModifierCapsLock;
    return (shift != capsLock ? 'A' : 'a') + code - 1;
}

TInt WindowsKeyCode(TUint aCode)
{
    switch (aCode) {
    case EKeyBackspace: return VK_BACK;
    case EKeyTab: return VK_TAB;
    case EKeyEnter: case EKeyDevice3: return VK_RETURN;
    case EKeyEscape: return VK_ESCAPE;
    case EKeyPageUp: return VK_PRIOR;
    case EKeyPageDown: return VK_NEXT;
    case EKeyHome: return VK_HOME;
    case EKeyEnd: return VK_END;
    case EKeyLeftArrow: return VK_LEFT;
    case EKeyUpArrow: return VK_UP;
    case EKeyRightArrow: return VK_RIGHT;
    case EKeyDownArrow: return VK_DOWN;
    case EKeyDelete: return VK_DELETE;
    }
    if (aCode >= 'a' && aCode <= 'z')
        return aCode - 'a' + 'A';
    if ((aCode >= 'A' && aCode <= 'Z') || (aCode >= '0' && aCode <= '9') || aCode == ' ')
        return aCode;
    return 0;
}

} // namespace

CWebContainer* CWebContainer::NewL(const TRect& aRect, MWebContainerObserver& aObserver)
{
    CWebContainer* self = new (ELeave) CWebContainer(aObserver);
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    CleanupStack::Pop(self);
    return self;
}

CWebContainer::CWebContainer(MWebContainerObserver& aObserver)
    : iObserver(aObserver)
{
}

void CWebContainer::ConstructL(const TRect& aRect)
{
    // The package installs UI resources on the executable's drive.
    TFileName executable = RProcess().FileName();
    TFileName resource = executable.Left(2);
    resource.Append(_L("\\resource\\apps\\browser_ui.rsc"));
    iUiResourceOffset = iCoeEnv->AddResourceFileL(resource);
    CreateWindowL();
    EnableDragEvents();
    SetRect(aRect);

    CAknEdwinState* state = new (ELeave) CAknEdwinState();
    iEditorState = state;
    state->SetObjectProvider(this);
    state->SetFlags(KEditorFlags);
    iExtendedInputCapabilities = CAknExtendedInputCapabilities::NewL();
    iExtendedInputCapabilities->SetEditorType(CAknExtendedInputCapabilities::EEdwinBased);

    // SetRequiredDisplayMode does not enable window transparency on NGA.
    Window().SetRequiredDisplayMode(EColor16MA);
#if SYMBIAN_BROWSER_MEDIA && !SYMBIAN_BROWSER_SCREENPLAY
    // Preserve the page's alpha-zero video holes over MMF background surfaces,
    // including the window server's clear before a redraw.
    User::LeaveIfError(Window().SetTransparencyAlphaChannel());
    Window().SetBackgroundColor(TRgb(0, 0, 0, 0));
    iNativeVideoWindowPrepared = true;
    iNativeVideoWindowError = KErrNone;
#elif SYMBIAN_BROWSER_SCREENPLAY
    // ScreenPlay's GetBackgroundSurface contract forbids alpha windows. Keep
    // this window opaque until the first native video requires the alpha path.
    Window().SetBackgroundColor(KRgbWhite);
#endif
    TInt digitizer = 0;
    TInt enabledPointers = 0;
    TInt pointerQuery = KErrNotSupported;
    unsigned capacity = 0;
    if (HAL::Get(HALData::EPen, digitizer) == KErrNone && digitizer) {
        pointerQuery = HAL::Get(HALData::EPointerNumberOfPointers, enabledPointers);
        // The native contract specifies one legacy pointer when this attribute
        // is unavailable. Zero on a successful query means none enabled.
        if (pointerQuery != KErrNone)
            enabledPointers = 1;
        if (enabledPointers > 0)
            capacity = static_cast<unsigned>(enabledPointers) < maximumSupportedTouchPoints ? static_cast<unsigned>(enabledPointers) : maximumSupportedTouchPoints;
        if (capacity > 1)
            Window().EnableAdvancedPointers();
        if (HAL::Get(HALData::EPointer3DMaxPressure, iMaximumTouchPressure) != KErrNone)
            iMaximumTouchPressure = 0;
    }
    iWebView = WebView::create(*this, Size().iWidth, Size().iHeight);
    capacity = iWebView->setTouchCapabilities(capacity);
    iTouchTracker = SymbianBrowser::TouchTracker(capacity);
    BrowserLog("touch: digitizer=%d enabled=%d query=%d accepted=%u", digitizer, enabledPointers, pointerQuery, capacity);
    ActivateL();
    // EGL consumes an RWindow pointer, not the window server's handle.
    iCoeEnv->WsSession().Finish();
#if SYMBIAN_BROWSER_MEDIA
    CreateVideoWindow();
    WebView::setMediaPlayerFactory(CreateMediaPlayer, static_cast<MNativeVideoWindow*>(this));
#endif
#if SYMBIAN_BROWSER_COMPOSITING && SYMBIAN_BROWSER_TRY_EGL
    // Every page is composited with EGL and scrolls cached GPU tiles. For
    // comparison, C:\Data\WebKit\no-gpu-tiles keeps ordinary pages on the
    // ScreenPlay/bitmap path.
    TEntry entry;
    if (iCoeEnv->FsSession().Entry(_L("C:\\Data\\WebKit\\no-gpu-tiles"), entry) == KErrNone) {
        WebView::setTiledCompositingEnabled(false);
        BrowserLog("compositing: GPU tiles off (no-gpu-tiles)");
    }
    iWebView->setNativeWindow(&Window());
#endif
}

CWebContainer::~CWebContainer()
{
    PrepareForExit();
    if (iWebView)
        iWebView->setNativeWindow(nullptr);
    delete iWebView;
    if (iAuthenticationUI)
        iAuthenticationUI->Release();
#if SYMBIAN_BROWSER_MEDIA
    WebView::setMediaPlayerFactory(nullptr, nullptr);
    // After the players, which display in it.
    if (iVideoUnderlay) {
        WebView::setCompositedVideoEnabled(false);
        delete iVideoUnderlay;
        iVideoUnderlay = nullptr;
    }
#endif
    delete iBitmap;
    delete iEditorState;
    delete iExtendedInputCapabilities;
    User::Free(iInlineText);
    delete iSelfTest;
    if (iUiResourceOffset >= 0)
        iCoeEnv->DeleteResourceFile(iUiResourceOffset);
}

void CWebContainer::PrepareForExit()
{
    iClosing = true;
#if SYMBIAN_BROWSER_SCREENPLAY
    DetachScreenPlay();
#endif
    cancelAuthenticationChallenge(0);
    if (iWebView)
        iWebView->detachDownloads();
    iDownloadIdentifier = 0;
    iTouchTracker.clear();
    if (iWebView)
        iWebView->cancelTouches();
    if (iSelfTest)
        iSelfTest->Cancel();
}

void CWebContainer::EnsureBitmapL()
{
    if (iBitmap && iBitmap->SizeInPixels() == Size())
        return;
    delete iBitmap;
    iBitmap = nullptr;
    CFbsBitmap* bitmap = new (ELeave) CFbsBitmap;
    CleanupStack::PushL(bitmap);
    User::LeaveIfError(bitmap->Create(Size(), EColor16MAP));
    CleanupStack::Pop(bitmap);
    iBitmap = bitmap;
    iDirty.Reset(TRect(Size()));
}

void CWebContainer::SizeChanged()
{
    if (iClosing)
        return;
    iDirty.Reset(TRect(Size()));
#if SYMBIAN_BROWSER_SCREENPLAY
    // The split view input relays the view out several times and makes it
    // shorter; recreating and attaching a surface costs 30-80 ms each on the
    // Nokia 808. Keep the presenter while its surface still fits.
    if (iScreenPlay && !iScreenPlay->Fits(Size()))
        DetachScreenPlay();
#endif
#if SYMBIAN_BROWSER_MEDIA
    if (iVideoUnderlay)
        iVideoUnderlay->SetRect(TRect(Window().Position(), Size()));
#endif
    if (!iWebView)
        return;
    iCoeEnv->WsSession().Finish();
    iWebView->setSize(Size().iWidth, Size().iHeight);
    // The split view input opened or closed: the view now ends above it.
    if (iEditable)
        RevealCaret();
}

void CWebContainer::Draw(const TRect& aRect) const
{
    if (iClosing)
        return;
    if (iWebView && iWebView->usesAcceleratedCompositing()) {
#if SYMBIAN_BROWSER_SCREENPLAY
        DetachScreenPlay();
#endif
        iWebView->renderCompositedFrame();
        if (iWebView->usesAcceleratedCompositing())
            return;
        // A failed EGL frame disables compositing and repaints the full bitmap.
        iDirty.Reset(TRect(Size()));
    }
    CWindowGc& gc = SystemGc();
#if SYMBIAN_BROWSER_SCREENPLAY
    if (iWebView && iWebView->requiresNativeVideoWindow()) {
#if SYMBIAN_BROWSER_MEDIA
        // Do this before painting can add MMF's background surface, while
        // GetBackgroundSurface is still legal on our opaque page window.
        const_cast<CWebContainer*>(this)->PrepareNativeVideoWindow();
#else
        DetachScreenPlay();
#endif
    } else if (iWebView && !iScreenPlayFailed) {
        if (!iScreenPlay) {
            const char* step = "allocate";
            TRAPD(error, iScreenPlay = CScreenPlaySurface::NewL(
                iCoeEnv->WsSession(), const_cast<CWebContainer*>(this)->Window(),
                iCoeEnv->ScreenDevice()->GetScreenNumber(),
                *const_cast<CWebContainer*>(this), &step));
            if (error != KErrNone) {
                BrowserLog("ScreenPlay: initialization unavailable (%s: %d), using bitmap",
                    step, error);
                iScreenPlayFailed = true;
            }
        }
        if (iScreenPlay) {
            CScreenPlaySurface::TFrame frame;
            // Lay out first so that the damage layout raises is painted in
            // this frame rather than the next.
            const TUint32 layoutStart = FrameCounter();
            iWebView->updateLayout();
            TUint32 marks[4] { FrameCounter() };
            TInt error = iScreenPlay->BeginFrame(Size(), iDirty, frame);
            bool currentFrame = error == KErrAlreadyExists;
            if (error == KErrInUse)
                return; // The availability AO requests another redraw.
            if (error == KErrNone) {
                // Cairo paints directly into this available surface buffer.
                // Keep invalidations raised during painting for the next frame.
                iDirty.Clear();
                marks[1] = FrameCounter();
                TInt pixels = 0;
                for (TInt i = 0; i < frame.iRegion.Count(); ++i) {
                    const TRect& rect = frame.iRegion[i];
                    iWebView->paint(frame.iPixels, frame.iStride,
                        { rect.iTl.iX, rect.iTl.iY, rect.Width(), rect.Height() });
                    pixels += rect.Width() * rect.Height();
                }
                marks[2] = FrameCounter();
                TUint32 submitMarks[5] { marks[2] };
                error = iScreenPlay->SubmitFrame(frame, iFrameStats ? submitMarks + 1 : nullptr);
                marks[3] = submitMarks[4] = FrameCounter();
                if (error == KErrNone) {
                    RecordFrame(iScreenPlayStats, marks, 3, &iScreenPlaySubmitStats, submitMarks);
                    RecordPaint(layoutStart, marks[0], marks[1], marks[2], frame.iRegion.Count(), pixels);
                }
            }
            if (error == KErrNone || currentFrame) {
                // Discard retained bitmap drawing so the background surface is
                // visible. WriteAlpha preserves transparency for system UI.
                // Transparent black is the composition key; RWindowBase::KeyColor
                // is not exported by every firmware (the Nokia 808 lacks it).
                gc.SetDrawMode(CGraphicsContext::EDrawModeWriteAlpha);
                gc.SetBrushColor(TRgb(0, 0, 0, 0));
                gc.Clear(aRect);
                gc.SetDrawMode(CGraphicsContext::EDrawModePEN);
                if (!iScreenPlayPresented) {
                    iScreenPlayPresented = true;
                    TPoint origin = PositionRelativeToScreen();
                    BrowserLog("ScreenPlay: surface submitted %dx%d at %d,%d",
                        Size().iWidth, Size().iHeight, origin.iX, origin.iY);
                    // The current redraw can be clipped. Clear any retained
                    // bitmap outside it in one subsequent full-window redraw.
                    const_cast<CWebContainer*>(this)->DrawDeferred();
                }
                return;
            }
            BrowserLog("ScreenPlay: frame failed (%d), using bitmap", error);
            iScreenPlayFailed = true;
            DetachScreenPlay();
            iDirty.Reset(TRect(Size()));
        }
    }
#endif
    TRAP_IGNORE(const_cast<CWebContainer*>(this)->EnsureBitmapL());
    if (!iBitmap) {
        gc.SetBrushColor(KRgbWhite);
        gc.Clear(aRect);
        return;
    }
    TUint32 marks[3] { };
    bool painted = !iDirty.IsEmpty();
    if (painted) {
        marks[0] = FrameCounter();
        // The engine paints into the bitmap's pixels directly; EColor16MAP
        // is the premultiplied 0xAARRGGBB layout it expects.
        const TDamageRegion dirty = iDirty;
        iDirty.Clear();
        iBitmap->BeginDataAccess();
        uint8_t* pixels = reinterpret_cast<uint8_t*>(iBitmap->DataAddress());
        int stride = CFbsBitmap::ScanLineLength(iBitmap->SizeInPixels().iWidth, EColor16MAP);
        for (TInt i = 0; i < dirty.Count(); ++i) {
            const TRect& rect = dirty[i];
            iWebView->paint(pixels, stride, { rect.iTl.iX, rect.iTl.iY, rect.Width(), rect.Height() });
        }
        iBitmap->EndDataAccess();
        marks[1] = FrameCounter();
    }
    TRect source(aRect.iTl - Rect().iTl, aRect.Size());
    gc.SetDrawMode(CGraphicsContext::EDrawModeWriteAlpha);
    gc.BitBlt(aRect.iTl, iBitmap, source);
    gc.SetDrawMode(CGraphicsContext::EDrawModePEN);
    if (painted) {
        // BitBlt queues a window server command; this times the client side.
        marks[2] = FrameCounter();
        RecordFrame(iBitmapStats, marks, 2);
    }
}

#if SYMBIAN_BROWSER_SCREENPLAY
void CWebContainer::ScreenPlaySurfaceReady()
{
    if (!iClosing && iScreenPlay)
        DrawDeferred();
}

void CWebContainer::DetachScreenPlay() const
{
    if (!iScreenPlay)
        return;
    // Detach checks the current background ID before removing it: MMF may
    // already have replaced our surface with its own native video surface.
    delete iScreenPlay;
    iScreenPlay = nullptr;
    iScreenPlayPresented = false;
    iDirty.Reset(TRect(Size()));
}
#endif

#if SYMBIAN_BROWSER_MEDIA
namespace {
// A top-level window under the page's, filled black where no video is.
class CVideoUnderlay final : public CCoeControl {
public:
    void ConstructL(RWindow& aPageWindow, const TRect& aRect)
    {
        CreateWindowL();
        Window().SetBackgroundColor(KRgbBlack);
        SetRect(aRect);
        // Just behind the page window: a new window is in front of its
        // siblings, and moving it to the page's place puts it after it.
        Window().SetOrdinalPosition(aPageWindow.OrdinalPosition());
        // Pointer events go to the page window above, which covers it.
        Window().SetPointerGrab(EFalse);
        // Shown with the first video; until then nothing would see it.
        MakeVisible(EFalse);
        ActivateL();
    }

private:
    void Draw(const TRect& aRect) const override
    {
        CWindowGc& gc = SystemGc();
        gc.SetBrushColor(KRgbBlack);
        gc.Clear(aRect);
    }
};
}

void CWebContainer::CreateVideoWindow()
{
    // C:\Data\WebKit\no-composited-video keeps video in the page window,
    // which then goes to software painting while a video is shown.
    TEntry entry;
    if (iCoeEnv->FsSession().Entry(_L("C:\\Data\\WebKit\\no-composited-video"), entry) == KErrNone) {
        BrowserLog("video: composited video off (no-composited-video)");
        return;
    }
    auto* underlay = new CVideoUnderlay;
    if (!underlay) {
        BrowserLog("video: video window unavailable (%d)", KErrNoMemory);
        return;
    }
    TRAPD(error, underlay->ConstructL(Window(), TRect(Window().Position(), Size())));
    if (error != KErrNone) {
        delete underlay;
        BrowserLog("video: video window unavailable (%d)", error);
        return;
    }
    iVideoUnderlay = underlay;
    WebView::setCompositedVideoEnabled(true);
}

RWindow& CWebContainer::VideoWindow()
{
    if (iVideoUnderlay)
        return static_cast<RWindow&>(*iVideoUnderlay->DrawableWindow()); // CreateWindowL() makes an RWindow.
    return Window();
}

void CWebContainer::setVideoUnderlayVisible(bool aVisible)
{
    if (!iVideoUnderlay)
        return;
    // The EGL surface's alpha then reaches the composition, and the video
    // window under it shows through the holes.
    Window().SetSurfaceTransparency(aVisible);
    BrowserLog("video: page surface %s", aVisible ? "transparent" : "opaque");
}

TInt CWebContainer::PrepareNativeVideoWindow()
{
    if (iVideoUnderlay) {
        iVideoUnderlay->MakeVisible(ETrue);
        // A composited page draws holes for the video in its EGL frames.
        if (iWebView && iWebView->usesAcceleratedCompositing())
            return KErrNone;
    }
    if (iNativeVideoWindowPrepared)
        return iNativeVideoWindowError;
    iNativeVideoWindowPrepared = true;
    // MMF can prepare a display before the next page redraw. Retire EGL now,
    // before it can destroy a window surface subsequently attached by MMF.
    // Also stops EGL composition from starting later on this window.
    if (iWebView)
        iWebView->setNativeWindow(nullptr);
#if SYMBIAN_BROWSER_SCREENPLAY
    // Retire the page surface while the window is still opaque. Alpha cannot
    // be undone reliably on NGA, so this window stays on the bitmap/video path.
    DetachScreenPlay();
    iScreenPlayFailed = true;
#endif
    iNativeVideoWindowError = Window().SetTransparencyAlphaChannel();
    if (iNativeVideoWindowError == KErrNone)
        Window().SetBackgroundColor(TRgb(0, 0, 0, 0));
    else
        BrowserLog("video: alpha window unavailable (%d)", iNativeVideoWindowError);
    iDirty.Reset(TRect(Size()));
    DrawDeferred();
    return iNativeVideoWindowError;
}
#endif

void CWebContainer::FocusChanged(TDrawNow)
{
    if (!iClosing && iWebView)
        iWebView->setFocused(IsFocused());
}

void CWebContainer::HandlePointerEventL(const TPointerEvent& aPointerEvent)
{
    if (iClosing)
        return;
    TPoint point = aPointerEvent.iPosition - Rect().iTl;
    if (iTouchTracker.capacity()) {
        const auto* advanced = aPointerEvent.AdvancedPointerEvent();
        TInt nativeIdentifier = advanced ? advanced->PointerNumber() : 0;
        if (nativeIdentifier < 0)
            return;
        unsigned identifier = static_cast<unsigned>(nativeIdentifier);
        if (aPointerEvent.iType == TPointerEvent::EOutOfRange) {
            if (iTouchTracker.contains(identifier))
                iWebView->cancelTouches();
            return;
        }
        TouchState state;
        switch (aPointerEvent.iType) {
        case TPointerEvent::EButton1Down:
            iObserver.WebPointerDownL();
            state = TouchState::Pressed;
            break;
        case TPointerEvent::EDrag:
            state = TouchState::Moved;
            break;
        case TPointerEvent::EButton1Up:
            state = TouchState::Released;
            break;
        default:
            return;
        }
        TPoint screen = PositionRelativeToScreen() + point;
        float pressure = 0.5f;
        if (advanced && iMaximumTouchPressure > 0)
            pressure = static_cast<float>(Min(Max(advanced->Pressure(), 0), iMaximumTouchPressure)) / iMaximumTouchPressure;
        SymbianBrowser::TouchFrame frame;
        if (!iTouchTracker.update(state, identifier, point.iX, point.iY, screen.iX, screen.iY, pressure, frame))
            return;
        unsigned modifiers = 0;
        if (aPointerEvent.iModifiers & EModifierShift)
            modifiers |= ShiftKey;
        if (aPointerEvent.iModifiers & EModifierCtrl)
            modifiers |= ControlKey;
        if (aPointerEvent.iModifiers & EModifierAlt)
            modifiers |= AltKey;
        bool tapped = iWebView->handleTouch(frame.points.data(), frame.count, modifiers);
        if (tapped && iEditable && iWebView->lastTapWasOnFocusedEditableElement())
            RequestTouchInputL();
        return;
    }
    switch (aPointerEvent.iType) {
    case TPointerEvent::EButton1Down:
        iObserver.WebPointerDownL();
        iPointerDown = point;
        iWebView->handlePointer(PointerEvent::Down, point.iX, point.iY);
        break;
    case TPointerEvent::EDrag:
        iWebView->handlePointer(PointerEvent::Move, point.iX, point.iY);
        break;
    case TPointerEvent::EButton1Up: {
        iWebView->handlePointer(PointerEvent::Up, point.iX, point.iY);
        TPoint moved = point - iPointerDown;
        TBool tap = Abs(moved.iX) <= KTapSlop && Abs(moved.iY) <= KTapSlop;
        // Tapping a field opens the touch keyboard, as it does for an edwin.
        if (tap && iEditable && iWebView->lastTapWasOnFocusedEditableElement())
            RequestTouchInputL();
        break;
    }
    default:
        break;
    }
}

TKeyResponse CWebContainer::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
{
    if (iClosing || aType != EEventKey)
        return EKeyWasNotConsumed;
    TUint code = KeyCodeForEvent(aKeyEvent.iCode, aKeyEvent.iScanCode, aKeyEvent.iModifiers);
    TInt keyCode = WindowsKeyCode(code);
    TBuf<1> text;
    // Characters, including those an input method sends as key events.
    if (code >= ' ' && code < ENonCharacterKeyBase && code != EKeyDelete)
        text.Append(TChar(code));
    else if (code == EKeyEnter || code == EKeyDevice3)
        text.Append(TChar('\r'));
    if (!keyCode && !text.Length())
        return EKeyWasNotConsumed;

    unsigned modifiers = KeyModifiers(aKeyEvent.iModifiers);
    iUpdatingForFep = ETrue;
    bool handled = iWebView->handleKey(true, keyCode, ToText(text), modifiers);
    // Text supplies the DOM key identity on keyup too; handleKey never emits
    // a character event for a release or a Control shortcut.
    iWebView->handleKey(false, keyCode, ToText(text), modifiers);
    iUpdatingForFep = EFalse;
    if (iEditable)
        ReportEditorEventL(MAknEdStateObserver::EAknSyncEdwinState);
    return handled || iEditable ? EKeyWasConsumed : EKeyWasNotConsumed;
}

TCoeInputCapabilities CWebContainer::InputCapabilities() const
{
    if (iClosing || !iEditable)
        return TCoeInputCapabilities(TCoeInputCapabilities::ENone);
    TUint capabilities = TCoeInputCapabilities::ENavigation;
    switch (iWebView->focusedInputType()) {
    case InputType::Password:
        capabilities |= TCoeInputCapabilities::ESecretText;
        break;
    case InputType::Number:
        capabilities |= TCoeInputCapabilities::EWesternNumericReal;
        break;
    default:
        capabilities |= TCoeInputCapabilities::EAllText;
        break;
    }
    CWebContainer* self = const_cast<CWebContainer*>(this);
    TCoeInputCapabilities inputCapabilities(capabilities, static_cast<MCoeFepAwareTextEditor*>(self), nullptr);
    inputCapabilities.SetObjectProvider(self);
    return inputCapabilities;
}

TTypeUid::Ptr CWebContainer::MopSupplyObject(TTypeUid aId)
{
    if (aId.iUid == CAknExtendedInputCapabilities::ETypeId)
        return aId.MakePtr(iExtendedInputCapabilities);
    return CCoeControl::MopSupplyObject(aId);
}

void CWebContainer::UpdateEditorStateL()
{
    InputType type = iWebView->focusedInputType();
    iEditable = type != InputType::None;
    if (iInlineEditing && !iEditable) {
        iInlineEditing = EFalse;
        User::Free(iInlineText);
        iInlineText = nullptr;
    }

    CAknEdwinState* state = static_cast<CAknEdwinState*>(iEditorState);
    if (iEditable && state && iEditorStateUid == KNullUid) {
        TInt flags = 0;
        TInt defaultMode = EAknEditorTextInputMode;
        TInt permittedModes = EAknEditorAllInputModes;
        TInt defaultCase = EAknEditorTextCase;
        switch (type) {
        case InputType::Password:
            flags = EAknEditorFlagNoT9 | EAknEditorFlagLatinInputModesOnly;
            defaultMode = EAknEditorSecretAlphaInputMode;
            permittedModes = EAknEditorSecretAlphaInputMode | EAknEditorNumericInputMode;
            defaultCase = EAknEditorLowerCase;
            break;
        case InputType::Number:
        case InputType::Telephone:
            defaultMode = EAknEditorNumericInputMode;
            permittedModes = EAknEditorNumericInputMode;
            break;
        case InputType::Email:
        case InputType::URL:
            flags = EAknEditorFlagNoT9 | EAknEditorFlagLatinInputModesOnly;
            defaultCase = EAknEditorLowerCase;
            break;
        default:
            break;
        }
        // The touch keyboard over the lower part of the screen, the page
        // still visible above it, rather than a full screen editor.
        flags |= KEditorFlags;
        state->SetFlags(flags);
        state->SetDefaultInputMode(defaultMode);
        state->SetCurrentInputMode(defaultMode);
        state->SetPermittedInputModes(permittedModes);
        state->SetDefaultCase(defaultCase);
        state->SetCurrentCase(defaultCase);
        state->SetPermittedCases(EAknEditorAllCaseModes);
        state->SetNumericKeymap(EAknEditorStandardNumberModeKeymap);
        // A FEP already attached to the page view reads the new state.
        state->ReportAknEdStateEventL(MAknEdStateObserver::EAknEdwinStateFlagsUpdate);
        state->ReportAknEdStateEventL(MAknEdStateObserver::EAknEdwinStateInputModeUpdate);
        state->ReportAknEdStateEventL(MAknEdStateObserver::EAknEdwinStateCaseModeUpdate);
    }
    // The FEP asks for InputCapabilities() again.
    iCoeEnv->InputCapabilitiesChanged();
}

void CWebContainer::ReportEditorEventL(TInt aEvent)
{
    if (iEditorState && iEditorStateUid == KNullUid)
        static_cast<CAknEdwinState*>(iEditorState)->ReportAknEdStateEventL(static_cast<MAknEdStateObserver::EAknEdwinStateEvent>(aEvent));
}

void CWebContainer::RequestTouchInputL()
{
    iExtendedInputCapabilities->ReportEventL(CAknExtendedInputCapabilities::EActivatePenInputRequest, nullptr);
}

// WebViewClient. These run inside the engine, which must not be left
// through, so every leave stops here.

void CWebContainer::didInvalidate(const WebKitSymbian::Rect& aRect)
{
    if (iClosing)
        return;
    TRect rect(TPoint(aRect.x, aRect.y), TSize(aRect.width, aRect.height));
    rect.Intersection(TRect(Size()));
    if (rect.IsEmpty())
        return;
    iDirty.Add(rect);
    Window().Invalidate(TRect(rect.iTl + Rect().iTl, rect.Size()));
}

bool CWebContainer::didScroll(const WebKitSymbian::Rect& aArea, int aDx, int aDy)
{
#if SYMBIAN_BROWSER_SCREENPLAY
    if (iClosing || !iScreenPlay)
        return false;
    TRect area(TPoint(aArea.x, aArea.y), TSize(aArea.width, aArea.height));
    TPoint delta(aDx, aDy);
    if (!iScreenPlay->ScrollFrame(Size(), area, delta))
        return false;
    // Pixels invalidated but not yet painted moved too.
    iDirty.Scroll(area, delta);
    return true;
#else
    (void)aArea;
    (void)aDx;
    (void)aDy;
    return false;
#endif
}

void CWebContainer::didChangeTitle(Text aTitle)
{
    if (iClosing)
        return;
    TRAP_IGNORE(iObserver.WebTitleChanged(ToDesC(aTitle)));
}

void CWebContainer::didChangeURL(Text aUrl)
{
    if (iClosing)
        return;
    TRAP_IGNORE(iObserver.WebUrlChanged(ToDesC(aUrl)));
}

void CWebContainer::didChangeProgress(bool aIsLoading, double aProgress)
{
    if (iClosing)
        return;
    TRAP_IGNORE(iObserver.WebProgressChanged(aIsLoading, static_cast<TInt>(aProgress * 100)));
}

void CWebContainer::didChangeEditingState()
{
    if (iClosing)
        return;
    TRAP_IGNORE(UpdateEditorStateL());
    if (iEditable && iSelfTest && !iSelfTest->IsActive())
        iSelfTest->CallBack();
}

void CWebContainer::didCancelTouchSequence()
{
    iTouchTracker.clear();
}

void CWebContainer::didChangeSelection()
{
    if (iClosing)
        return;
    if (iSplitInput && iEditable)
        RevealCaret();
    if (iUpdatingForFep || !iEditable)
        return;
    TRAP_IGNORE(ReportEditorEventL(MAknEdStateObserver::EAknSyncEdwinState));
}

void CWebContainer::HandleResourceChange(TInt aType)
{
    CCoeControl::HandleResourceChange(aType);
    if (aType == KAknSplitInputEnabled)
        iSplitInput = ETrue;
    else if (aType == KAknSplitInputDisabled)
        iSplitInput = EFalse;
}

// Scrolls the page to bring the caret into view, as the split view input
// opens over the lower part of the screen and the view shrinks above it.
void CWebContainer::RevealCaret()
{
    WebKitSymbian::Rect caret = iWebView->caretRect();
    TInt height = Size().iHeight;
    const TInt margin = 16;
    if (caret.y >= margin && caret.y + caret.height <= height - margin)
        return;
    iWebView->scrollBy(0, caret.y - Max(margin, (height - caret.height) / 2));
}

void CWebContainer::runJavaScriptAlert(Text aMessage)
{
    if (iClosing)
        return;
    TRAP_IGNORE(CEikonEnv::InfoWinL(ToDesC(aMessage), KNullDesC));
}

bool CWebContainer::runJavaScriptConfirm(Text aMessage)
{
    if (iClosing)
        return false;
    TBool result = EFalse;
    TRAP_IGNORE(result = CEikonEnv::QueryWinL(ToDesC(aMessage), KNullDesC));
    return result;
}

bool CWebContainer::runJavaScriptPrompt(Text aMessage, Text aDefaultValue, TextResult& aResult)
{
    if (iClosing)
        return false;
    // Keep a bounded native editor, without silently truncating the default.
    const TInt maximumLength = 4096;
    if (aDefaultValue.length > maximumLength) {
        BrowserLog("prompt cancelled: default exceeds native editor limit");
        return false;
    }
    TBool accepted = EFalse;
    TRAPD(error, {
        HBufC* buffer = HBufC::NewLC(maximumLength);
        TPtr value = buffer->Des();
        value.Copy(ToDesC(aDefaultValue));
        CAknTextQueryDialog* dialog = CAknTextQueryDialog::NewL(value);
        dialog->SetMaxLength(maximumLength);
        // ExecuteLD owns/destroys the dialog, including on Leave. The resource
        // uses EDataLayoutAllowEmptyInput, so OK on empty text is not Cancel.
        accepted = dialog->ExecuteLD(R_BROWSER_PROMPT, ToDesC(aMessage));
        if (accepted)
            aResult.setText(ToText(value));
        CleanupStack::PopAndDestroy(buffer);
    });
    if (error)
        BrowserLog("prompt cancelled: native error %d", error);
    return !error && accepted;
}

bool CWebContainer::runAuthenticationChallenge(uint64_t identifier, Text origin, unsigned attempt, TextResult& username, TextResult& password)
{
    if (iClosing)
        return false;
    if (!iAuthenticationUI) {
        TRAPD(error, iAuthenticationUI = new (ELeave) CAuthenticationUI);
        if (error)
            return false;
    }
    // Run retains its controller; the container may close in the nested loop.
    return iAuthenticationUI->Run(identifier, origin, attempt, username, password);
}

void CWebContainer::cancelAuthenticationChallenge(uint64_t identifier)
{
    if (iAuthenticationUI)
        iAuthenticationUI->Cancel(identifier);
}

bool CWebContainer::chooseFile(TextResult& aResult)
{
    if (iClosing)
        return false;
    TBool accepted = EFalse;
    TRAPD(error, {
        TFileName path;
        const TInt memories = AknCommonDialogsDynMem::EMemoryTypePhone
            | AknCommonDialogsDynMem::EMemoryTypeInternalMassStorage
            | AknCommonDialogsDynMem::EMemoryTypeMMCExternal;
        // A zero resource ID uses the firmware's defaults. The dynamic-memory
        // facade lists actual local drives, then offers single-file selection.
        accepted = AknCommonDialogsDynMem::RunSelectDlgLD(memories, path, 0);
        if (accepted) {
            RFs fs;
            User::LeaveIfError(fs.Connect());
            CleanupClosePushL(fs);
            RFile file;
            User::LeaveIfError(file.Open(fs, path, EFileRead | EFileShareReadersOnly));
            file.Close();
            aResult.setText(ToText(path));
            CleanupStack::PopAndDestroy(&fs);
        }
    });
    if (error)
        BrowserLog("file chooser cancelled: native error %d", error);
    return !error && accepted;
}

void CWebContainer::didStartDownload(uint64_t identifier)
{
    if (iClosing)
        return;
    iDownloadIdentifier = identifier;
    TRAP_IGNORE(iObserver.WebDownloadProgress(ETrue, 0, -1));
}

int CWebContainer::chooseDownloadDestination(uint64_t identifier, Text, Text suggestion, TextResult& result)
{
    if (iClosing)
        return 1;
    iDownloadIdentifier = identifier;
    int error = 0;
    bool accepted = ChooseNativeDownloadDestination(suggestion, result, error);
    // The native modal loop may destroy this container. Only use local values
    // after it returns; the download manager rechecks its client and transfer.
    if (error)
        BrowserLog("download save dialog failed: %d", error);
    return error ? error : (accepted ? 0 : 1);
}

int CWebContainer::didCloseDownloadDestinationUI()
{
    if (iClosing)
        return KErrCancel;
    TRAPD(error, iObserver.WebNativeDialogClosedL());
    // Native restoration may invalidate the client. Only return the local
    // status; the manager rechecks attachment before using its client.
    return error;
}

DownloadFile* CWebContainer::createDownloadFile()
{
    return iClosing ? nullptr : createNativeDownloadFile();
}

void CWebContainer::CancelDownload()
{
    if (!iClosing && iWebView && iDownloadIdentifier)
        iWebView->cancelDownload(iDownloadIdentifier);
}

void CWebContainer::didChangeDownloadProgress(uint64_t identifier, uint64_t received, int64_t total)
{
    if (iClosing)
        return;
    iDownloadIdentifier = identifier;
    TRAP_IGNORE(iObserver.WebDownloadProgress(ETrue, received, total));
    BrowserLog("download %llu: %llu/%lld bytes", static_cast<unsigned long long>(identifier), static_cast<unsigned long long>(received), static_cast<long long>(total));
}

void CWebContainer::didFinishDownload(uint64_t identifier, Text path, DownloadFailure failure, int error, int cleanupError, Text description)
{
    if (iClosing)
        return;
    if (identifier == iDownloadIdentifier) {
        iDownloadIdentifier = 0;
        TRAP_IGNORE(iObserver.WebDownloadProgress(EFalse, 0, -1));
    }
    BrowserLog("download %llu: terminal=%u error=%d cleanup=%d", static_cast<unsigned long long>(identifier), static_cast<unsigned>(failure), error, cleanupError);
    ShowNativeDownloadResult(path, failure, error, cleanupError, description);
}

int CWebContainer::runSelectPopup(const Text* aItems, int aCount, int aSelected)
{
    if (iClosing)
        return -1;
    TInt result = -1;
    TRAP_IGNORE(result = iObserver.WebSelectPopupL(aItems, aCount, aSelected));
    return result;
}

// MCoeFepAwareTextEditor: the FEP's inline text becomes the page's
// composition, which WebCore shows underlined in the field.

void CWebContainer::StartFepInlineEditL(const TDesC& aInitialInlineText, TInt aPositionOfInsertionPointInInlineText,
    TBool, const MFormCustomDraw*, MFepInlineTextFormatRetriever&, MFepPointerEventHandlerDuringInlineEdit&)
{
    iInlineEditing = ETrue;
    UpdateFepInlineTextL(aInitialInlineText, aPositionOfInsertionPointInInlineText);
}

void CWebContainer::StartFepInlineEditL(TBool& aSetToTrue, const TCursorSelection& aCursorSelection,
    const TDesC& aInitialInlineText, TInt aPositionOfInsertionPointInInlineText, TBool aCursorVisibility,
    const MFormCustomDraw* aCustomDraw, MFepInlineTextFormatRetriever& aFormatRetriever,
    MFepPointerEventHandlerDuringInlineEdit& aPointerHandler)
{
    aSetToTrue = ETrue;
    // The inline text replaces this selection.
    iUpdatingForFep = ETrue;
    iWebView->setSelection(aCursorSelection.iAnchorPos, aCursorSelection.iCursorPos);
    iUpdatingForFep = EFalse;
    StartFepInlineEditL(aInitialInlineText, aPositionOfInsertionPointInInlineText, aCursorVisibility,
        aCustomDraw, aFormatRetriever, aPointerHandler);
}

void CWebContainer::UpdateFepInlineTextL(const TDesC& aNewInlineText, TInt aPositionOfInsertionPointInInlineText)
{
    HBufC* text = aNewInlineText.AllocL();
    User::Free(iInlineText);
    iInlineText = text;
    iUpdatingForFep = ETrue;
    iWebView->setComposition(ToText(*iInlineText), aPositionOfInsertionPointInInlineText);
    iUpdatingForFep = EFalse;
}

void CWebContainer::SetInlineEditingCursorVisibilityL(TBool)
{
}

void CWebContainer::CancelFepInlineEdit()
{
    if (!iInlineEditing)
        return;
    iInlineEditing = EFalse;
    iUpdatingForFep = ETrue;
    iWebView->cancelComposition();
    iUpdatingForFep = EFalse;
    User::Free(iInlineText);
    iInlineText = nullptr;
}

void CWebContainer::DoCommitFepInlineEditL()
{
    if (!iInlineEditing)
        return;
    iInlineEditing = EFalse;
    iUpdatingForFep = ETrue;
    iWebView->confirmComposition(iInlineText ? ToText(*iInlineText) : Text { });
    iUpdatingForFep = EFalse;
    User::Free(iInlineText);
    iInlineText = nullptr;
}

TInt CWebContainer::DocumentLengthForFep() const
{
    return iWebView->editableTextLength();
}

TInt CWebContainer::DocumentMaximumLengthForFep() const
{
    return iWebView->maximumEditableTextLength();
}

void CWebContainer::SetCursorSelectionForFepL(const TCursorSelection& aCursorSelection)
{
    iUpdatingForFep = ETrue;
    iWebView->setSelection(aCursorSelection.iAnchorPos, aCursorSelection.iCursorPos);
    iUpdatingForFep = EFalse;
}

void CWebContainer::GetCursorSelectionForFep(TCursorSelection& aCursorSelection) const
{
    int anchor = 0;
    int focus = 0;
    iWebView->selection(anchor, focus);
    aCursorSelection.SetSelection(focus, anchor);
}

void CWebContainer::GetEditorContentForFep(TDes& aEditorContent, TInt aDocumentPosition, TInt aLengthToRetrieve) const
{
    TInt length = Min(aLengthToRetrieve, aEditorContent.MaxLength());
    if (length <= 0 || aDocumentPosition < 0) {
        aEditorContent.Zero();
        return;
    }
    uint16_t* buffer = reinterpret_cast<uint16_t*>(const_cast<TText*>(aEditorContent.Ptr()));
    aEditorContent.SetLength(iWebView->copyEditableText(aDocumentPosition, length, buffer));
}

void CWebContainer::GetFormatForFep(TCharFormat& aFormat, TInt) const
{
    aFormat = TCharFormat();
}

void CWebContainer::GetScreenCoordinatesForFepL(TPoint& aLeftSideOfBaseLine, TInt& aHeight, TInt& aAscent, TInt) const
{
    WebKitSymbian::Rect caret = iWebView->caretRect();
    aHeight = caret.height;
    aAscent = caret.height * 4 / 5;
    aLeftSideOfBaseLine = PositionRelativeToScreen() + TPoint(caret.x, caret.y + aAscent);
}

MCoeFepAwareTextEditor_Extension1* CWebContainer::Extension1(TBool& aSetToTrue)
{
    aSetToTrue = ETrue;
    return this;
}

void CWebContainer::MCoeFepAwareTextEditor_Reserved_2()
{
}

// MCoeFepAwareTextEditor_Extension1: the editor keeps the FEP's state object.
// The AVKON FEP uses the CAknEdwinState stored under KNullUid.

void CWebContainer::SetStateTransferingOwnershipL(CState* aState, TUid aTypeSafetyUid)
{
    if (aState != iEditorState)
        delete iEditorState;
    iEditorState = aState;
    iEditorStateUid = aTypeSafetyUid;
}

MCoeFepAwareTextEditor_Extension1::CState* CWebContainer::State(TUid aTypeSafetyUid)
{
    return aTypeSafetyUid == iEditorStateUid ? iEditorState : nullptr;
}

void CWebContainer::SetCursorType(TBool& aSetToTrue, const TTextCursor&)
{
    aSetToTrue = ETrue;
}

MCoeFepLayDocExtension* CWebContainer::GetFepLayDocExtension(TBool&)
{
    return nullptr;
}

void CWebContainer::MCoeFepAwareTextEditor_Extension1_Reserved_4()
{
}

// Input self-test.

void CWebContainer::EnableFrameStats()
{
    if (iFrameStats)
        return;
    if (HAL::Get(HAL::EFastCounterFrequency, iFastCounterHz) != KErrNone || iFastCounterHz <= 0) {
        BrowserLog("frame-stats: no fast counter");
        return;
    }
    TInt countsUp = 1;
    if (HAL::Get(HAL::EFastCounterCountsUp, countsUp) == KErrNone)
        iFastCounterCountsUp = countsUp;
    iFrameStats = true;
    WebView::setFrameStatsEnabled(true);
    BrowserLog("frame-stats: enabled, fast counter %d Hz", iFastCounterHz);
}

TUint32 CWebContainer::FrameCounter() const
{
    return iFrameStats ? User::FastCounter() : 0;
}

void CWebContainer::RecordFrame(WebKitSymbian::FrameStats& aStats, const TUint32* aMarks, TInt aStages,
    WebKitSymbian::FrameStats* aDetail, const TUint32* aDetailMarks) const
{
    if (!iFrameStats)
        return;
    auto milliseconds = [this](TUint32 aFrom, TUint32 aTo) {
        TUint32 ticks = iFastCounterCountsUp ? aTo - aFrom : aFrom - aTo;
        return ticks * 1000.0 / iFastCounterHz;
    };
    double durations[WebKitSymbian::FrameStats::maximumStages] { };
    for (TInt i = 0; i < aStages; ++i)
        durations[i] = milliseconds(aMarks[i], aMarks[i + 1]);
    double since = iLastFrameCounter ? milliseconds(iLastFrameCounter, aMarks[aStages]) : 0;
    iLastFrameCounter = aMarks[aStages];
    iLastFrameInterval = since;
    if (aStats.add(durations, since)) {
        char summary[256];
        aStats.format(summary, sizeof(summary));
        BrowserLog("%s", summary);
    }
    if (aDetail) {
        double details[WebKitSymbian::FrameStats::maximumStages] { };
        for (TInt i = 0; i < 4; ++i)
            details[i] = milliseconds(aDetailMarks[i], aDetailMarks[i + 1]);
        if (aDetail->add(details, since)) {
            char summary[256];
            aDetail->format(summary, sizeof(summary));
            BrowserLog("%s", summary);
        }
    }
}

void CWebContainer::RecordPaint(TUint32 aLayoutStart, TUint32 aLaidOut, TUint32 aPaintStart,
    TUint32 aPainted, TInt aRects, TInt aPixels) const
{
    if (!iFrameStats)
        return;
    auto milliseconds = [this](TUint32 aFrom, TUint32 aTo) {
        TUint32 ticks = iFastCounterCountsUp ? aTo - aFrom : aFrom - aTo;
        return ticks * 1000.0 / iFastCounterHz;
    };
    const double values[WebKitSymbian::FrameStats::maximumStages] {
        milliseconds(aLayoutStart, aLaidOut), milliseconds(aPaintStart, aPainted), double(aRects), aPixels / 1000.0 };
    if (iScreenPlayPaintStats.add(values, iLastFrameInterval)) {
        char summary[256];
        iScreenPlayPaintStats.format(summary, sizeof(summary));
        BrowserLog("%s", summary);
    }
}

void CWebContainer::AllowMediaWithoutUserGesture()
{
    // Test harnesses cannot supply user activation before autorun pages play.
    iWebView->setMediaPlaybackRequiresUserGesture(false);
}

void CWebContainer::EnableInputSelfTest()
{
    if (!iSelfTest)
        iSelfTest = new CAsyncCallBack(TCallBack(RunInputSelfTest, this), CActive::EPriorityStandard);
}

TInt CWebContainer::RunInputSelfTest(TAny* aSelf)
{
    CWebContainer* self = static_cast<CWebContainer*>(aSelf);
    TRAPD(error, self->RunInputSelfTestL());
    RDebug::Printf("selftest: done (%d)", error);
    // Once only.
    CAsyncCallBack* callBack = self->iSelfTest;
    self->iSelfTest = nullptr;
    delete callBack;
    return 0;
}

void CWebContainer::RunInputSelfTestL()
{
    LogEditorState("focused");
    // Pinyin: the FEP shows the spelling inline, then commits the characters.
    iInlineEditing = ETrue;
    UpdateFepInlineTextL(_L("ni"), 2);
    LogEditorState("inline ni");
    UpdateFepInlineTextL(_L("nihao"), 5);
    LogEditorState("inline nihao");
    UpdateFepInlineTextL(_L("\x4F60\x597D"), 2);
    LogEditorState("inline nihao converted");
    DoCommitFepInlineEditL();
    LogEditorState("committed");
    // A character typed as a key event, as multi-tap input commits them.
    TKeyEvent key = { 'A', 0, 0, 0 };
    OfferKeyEventL(key, EEventKey);
    LogEditorState("key A");
    // Moving the cursor, then a cancelled inline edit.
    SetCursorSelectionForFepL(TCursorSelection(1, 1));
    LogEditorState("cursor 1");
    iInlineEditing = ETrue;
    UpdateFepInlineTextL(_L("xyz"), 3);
    LogEditorState("inline xyz");
    CancelFepInlineEdit();
    LogEditorState("cancelled");
    // Selecting text, then replacing it inline.
    SetCursorSelectionForFepL(TCursorSelection(3, 0));
    LogEditorState("select 0-3");
    iInlineEditing = ETrue;
    UpdateFepInlineTextL(_L("ok"), 2);
    DoCommitFepInlineEditL();
    LogEditorState("replaced");
    // Backspace.
    TKeyEvent backspace = { EKeyBackspace, EStdKeyBackspace, 0, 0 };
    OfferKeyEventL(backspace, EEventKey);
    LogEditorState("backspace");
    TPoint baseline;
    TInt height = 0;
    TInt ascent = 0;
    GetScreenCoordinatesForFepL(baseline, height, ascent, 0);
    RDebug::Printf("selftest: caret baseline %d,%d height %d", baseline.iX, baseline.iY, height);
}

void CWebContainer::LogEditorState(const char* aStep)
{
    TBuf<128> text;
    GetEditorContentForFep(text, 0, Min(DocumentLengthForFep(), text.MaxLength()));
    TBuf8<512> utf8;
    for (TInt i = 0; i < text.Length(); ++i) {
        TUint c = text[i];
        if (c < 0x80)
            utf8.Append(c);
        else if (c < 0x800) {
            utf8.Append(0xC0 | (c >> 6));
            utf8.Append(0x80 | (c & 0x3F));
        } else {
            utf8.Append(0xE0 | (c >> 12));
            utf8.Append(0x80 | ((c >> 6) & 0x3F));
            utf8.Append(0x80 | (c & 0x3F));
        }
    }
    TCursorSelection selection;
    GetCursorSelectionForFep(selection);
    RDebug::Printf("selftest: %s: \"%s\" length %d cursor %d anchor %d", aStep, utf8.PtrZ(),
        DocumentLengthForFep(), selection.iCursorPos, selection.iAnchorPos);
}
