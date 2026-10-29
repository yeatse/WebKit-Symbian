#include "WebContainer.h"

#include "NativeDeclarations.h"

#include <aknedsts.h>
#include <aknedstsobs.h>
#include <coemain.h>
#include <e32debug.h>
#include <e32keys.h>
#include <eikenv.h>
#include <fbs.h>
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
    CreateWindowL();
    EnableDragEvents();
    SetRect(aRect);

    CAknEdwinState* state = new (ELeave) CAknEdwinState();
    iEditorState = state;
    state->SetObjectProvider(this);
    state->SetFlags(KEditorFlags);
    iExtendedInputCapabilities = CAknExtendedInputCapabilities::NewL();
    iExtendedInputCapabilities->SetEditorType(CAknExtendedInputCapabilities::EEdwinBased);

    // The page leaves holes (alpha 0) where video plays in the window's
    // background, so the window keeps the bitmap's alpha.
    Window().SetRequiredDisplayMode(EColor16MA);
    iWebView = WebView::create(*this, Size().iWidth, Size().iHeight);
    ActivateL();
}

CWebContainer::~CWebContainer()
{
    delete iWebView;
    delete iBitmap;
    delete iEditorState;
    delete iExtendedInputCapabilities;
    User::Free(iInlineText);
    delete iSelfTest;
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
    iDirty = TRect(Size());
}

void CWebContainer::SizeChanged()
{
    TRAP_IGNORE(EnsureBitmapL());
    if (!iWebView)
        return;
    iWebView->setSize(Size().iWidth, Size().iHeight);
    // The split view input opened or closed: the view now ends above it.
    if (iEditable)
        RevealCaret();
}

void CWebContainer::Draw(const TRect& aRect) const
{
    CWindowGc& gc = SystemGc();
    if (!iBitmap) {
        gc.SetBrushColor(KRgbWhite);
        gc.Clear(aRect);
        return;
    }
    if (!iDirty.IsEmpty()) {
        // The engine paints into the bitmap's pixels directly; EColor16MAP
        // is the premultiplied 0xAARRGGBB layout it expects.
        WebKitSymbian::Rect dirty { iDirty.iTl.iX, iDirty.iTl.iY, iDirty.Width(), iDirty.Height() };
        iDirty = TRect();
        iBitmap->BeginDataAccess();
        uint8_t* pixels = reinterpret_cast<uint8_t*>(iBitmap->DataAddress());
        int stride = CFbsBitmap::ScanLineLength(iBitmap->SizeInPixels().iWidth, EColor16MAP);
        iWebView->paint(pixels, stride, dirty);
        iBitmap->EndDataAccess();
    }
    TRect source(aRect.iTl - Rect().iTl, aRect.Size());
    gc.SetDrawMode(CGraphicsContext::EDrawModeWriteAlpha);
    gc.BitBlt(aRect.iTl, iBitmap, source);
    gc.SetDrawMode(CGraphicsContext::EDrawModePEN);
}

void CWebContainer::FocusChanged(TDrawNow)
{
    if (iWebView)
        iWebView->setFocused(IsFocused());
}

void CWebContainer::HandlePointerEventL(const TPointerEvent& aPointerEvent)
{
    TPoint point = aPointerEvent.iPosition - Rect().iTl;
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
        if (tap && iEditable)
            RequestTouchInputL();
        break;
    }
    default:
        break;
    }
}

TKeyResponse CWebContainer::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
{
    if (aType != EEventKey)
        return EKeyWasNotConsumed;
    TInt keyCode = WindowsKeyCode(aKeyEvent.iCode);
    TBuf<1> text;
    // Characters, including those an input method sends as key events.
    if (aKeyEvent.iCode >= ' ' && aKeyEvent.iCode < ENonCharacterKeyBase && aKeyEvent.iCode != EKeyDelete)
        text.Append(TChar(aKeyEvent.iCode));
    else if (aKeyEvent.iCode == EKeyEnter || aKeyEvent.iCode == EKeyDevice3)
        text.Append(TChar('\r'));
    if (!keyCode && !text.Length())
        return EKeyWasNotConsumed;

    unsigned modifiers = 0;
    if (aKeyEvent.iModifiers & EModifierShift)
        modifiers |= ShiftKey;
    if (aKeyEvent.iModifiers & EModifierCtrl)
        modifiers |= ControlKey;
    if (aKeyEvent.iModifiers & EModifierAlt)
        modifiers |= AltKey;
    iUpdatingForFep = ETrue;
    bool handled = iWebView->handleKey(true, keyCode, ToText(text), modifiers);
    iWebView->handleKey(false, keyCode, Text { }, modifiers);
    iUpdatingForFep = EFalse;
    if (iEditable)
        ReportEditorEventL(MAknEdStateObserver::EAknSyncEdwinState);
    return handled || iEditable ? EKeyWasConsumed : EKeyWasNotConsumed;
}

TCoeInputCapabilities CWebContainer::InputCapabilities() const
{
    if (!iEditable)
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
    TRect rect(TPoint(aRect.x, aRect.y), TSize(aRect.width, aRect.height));
    rect.Intersection(TRect(Size()));
    if (rect.IsEmpty())
        return;
    if (iDirty.IsEmpty())
        iDirty = rect;
    else
        iDirty.BoundingRect(rect);
    Window().Invalidate(TRect(rect.iTl + Rect().iTl, rect.Size()));
}

void CWebContainer::didChangeTitle(Text aTitle)
{
    TRAP_IGNORE(iObserver.WebTitleChanged(ToDesC(aTitle)));
}

void CWebContainer::didChangeURL(Text aUrl)
{
    TRAP_IGNORE(iObserver.WebUrlChanged(ToDesC(aUrl)));
}

void CWebContainer::didChangeProgress(bool aIsLoading, double aProgress)
{
    TRAP_IGNORE(iObserver.WebProgressChanged(aIsLoading, static_cast<TInt>(aProgress * 100)));
}

void CWebContainer::didChangeEditingState()
{
    TRAP_IGNORE(UpdateEditorStateL());
    if (iEditable && iSelfTest && !iSelfTest->IsActive())
        iSelfTest->CallBack();
}

void CWebContainer::didChangeSelection()
{
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
    TRAP_IGNORE(CEikonEnv::InfoWinL(ToDesC(aMessage), KNullDesC));
}

bool CWebContainer::runJavaScriptConfirm(Text aMessage)
{
    TBool result = EFalse;
    TRAP_IGNORE(result = CEikonEnv::QueryWinL(ToDesC(aMessage), KNullDesC));
    return result;
}

int CWebContainer::runSelectPopup(const Text* aItems, int aCount, int aSelected)
{
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
