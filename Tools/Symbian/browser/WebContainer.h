// The control that shows a WebView. It paints the page through a bitmap,
// turns touch and keys into engine input, and is the FEP-aware editor for
// whatever field has the focus in the page, so AVKON input methods (the touch
// keyboard, handwriting, predictive and Chinese input) edit web text inline.
#pragma once

#include <WebKitSymbian.h>
#include <coecntrl.h>
#include <fepbase.h>

class CAknEdwinState;
class CAknExtendedInputCapabilities;
class CFbsBitmap;

class MWebContainerObserver {
public:
    virtual void WebTitleChanged(const TDesC& aTitle) = 0;
    virtual void WebUrlChanged(const TDesC& aUrl) = 0;
    virtual void WebProgressChanged(TBool aLoading, TInt aPercent) = 0;
    virtual TInt WebSelectPopupL(const WebKitSymbian::Text* aItems, TInt aCount, TInt aSelected) = 0;
};

// Resource change events for the split view input (Symbian^3 and later),
// which the SDK's headers do not have; Qt's S60 port uses the same values.
const TInt KAknSplitInputEnabled = 0x2001E2C0;
const TInt KAknSplitInputDisabled = 0x2001E2C1;

class CWebContainer : public CCoeControl
    , public WebKitSymbian::WebViewClient
    , public MCoeFepAwareTextEditor
    , public MCoeFepAwareTextEditor_Extension1 {
public:
    static CWebContainer* NewL(const TRect& aRect, MWebContainerObserver& aObserver);
    ~CWebContainer();

    WebKitSymbian::WebView& WebView() { return *iWebView; }

    // CCoeControl
    TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
    TCoeInputCapabilities InputCapabilities() const;

    // For trying inline editing where no input method runs (the emulator):
    // the first time a field gets the focus, the calls a FEP makes for
    // Chinese and Latin input are replayed and the field's text is logged
    // through RDebug after each.
    void EnableInputSelfTest();

private:
    CWebContainer(MWebContainerObserver& aObserver);
    void ConstructL(const TRect& aRect);

    // CCoeControl
    void Draw(const TRect& aRect) const;
    void SizeChanged();
    void FocusChanged(TDrawNow aDrawNow);
    void HandlePointerEventL(const TPointerEvent& aPointerEvent);
    void HandleResourceChange(TInt aType);
    TTypeUid::Ptr MopSupplyObject(TTypeUid aId);

    // WebKitSymbian::WebViewClient
    void didInvalidate(const WebKitSymbian::Rect&);
    void didChangeTitle(WebKitSymbian::Text);
    void didChangeURL(WebKitSymbian::Text);
    void didChangeProgress(bool isLoading, double progress);
    void didChangeEditingState();
    void didChangeSelection();
    void runJavaScriptAlert(WebKitSymbian::Text);
    bool runJavaScriptConfirm(WebKitSymbian::Text);
    int runSelectPopup(const WebKitSymbian::Text*, int count, int selectedIndex);

    // MCoeFepAwareTextEditor
    void StartFepInlineEditL(const TDesC& aInitialInlineText, TInt aPositionOfInsertionPointInInlineText,
        TBool aCursorVisibility, const MFormCustomDraw*, MFepInlineTextFormatRetriever&,
        MFepPointerEventHandlerDuringInlineEdit&);
    void UpdateFepInlineTextL(const TDesC& aNewInlineText, TInt aPositionOfInsertionPointInInlineText);
    void SetInlineEditingCursorVisibilityL(TBool aCursorVisibility);
    void CancelFepInlineEdit();
    TInt DocumentLengthForFep() const;
    TInt DocumentMaximumLengthForFep() const;
    void SetCursorSelectionForFepL(const TCursorSelection& aCursorSelection);
    void GetCursorSelectionForFep(TCursorSelection& aCursorSelection) const;
    void GetEditorContentForFep(TDes& aEditorContent, TInt aDocumentPosition, TInt aLengthToRetrieve) const;
    void GetFormatForFep(TCharFormat& aFormat, TInt aDocumentPosition) const;
    void GetScreenCoordinatesForFepL(TPoint& aLeftSideOfBaseLine, TInt& aHeight, TInt& aAscent, TInt aDocumentPosition) const;
    void DoCommitFepInlineEditL();
    MCoeFepAwareTextEditor_Extension1* Extension1(TBool& aSetToTrue);
    // fepbase.dll, which has these defaults, has no import proxy in the SDK.
    void MCoeFepAwareTextEditor_Reserved_2();

    // MCoeFepAwareTextEditor_Extension1
    void SetStateTransferingOwnershipL(CState* aState, TUid aTypeSafetyUid);
    CState* State(TUid aTypeSafetyUid);
    void StartFepInlineEditL(TBool& aSetToTrue, const TCursorSelection& aCursorSelection,
        const TDesC& aInitialInlineText, TInt aPositionOfInsertionPointInInlineText, TBool aCursorVisibility,
        const MFormCustomDraw*, MFepInlineTextFormatRetriever&, MFepPointerEventHandlerDuringInlineEdit&);
    void SetCursorType(TBool& aSetToTrue, const TTextCursor& aTextCursor);
    MCoeFepLayDocExtension* GetFepLayDocExtension(TBool& aSetToTrue);
    void MCoeFepAwareTextEditor_Extension1_Reserved_4();

    void UpdateEditorStateL();
    static TInt RunInputSelfTest(TAny* aSelf);
    void RunInputSelfTestL();
    void LogEditorState(const char* aStep);
    void ReportEditorEventL(TInt aEvent);
    void RequestTouchInputL();
    void EnsureBitmapL();
    void RevealCaret();

    MWebContainerObserver& iObserver;
    WebKitSymbian::WebView* iWebView { nullptr };
    CFbsBitmap* iBitmap { nullptr };
    mutable TRect iDirty;

    // The field's input method state, owned on behalf of the FEP; the AVKON
    // FEP's is a CAknEdwinState under KNullUid.
    CState* iEditorState { nullptr };
    TUid iEditorStateUid { KNullUid };
    CAknExtendedInputCapabilities* iExtendedInputCapabilities { nullptr };
    TBool iEditable { EFalse };
    HBufC* iInlineText { nullptr };
    TBool iInlineEditing { EFalse };
    // When the FEP replaces text itself, didChangeSelection() is ours.
    TBool iUpdatingForFep { EFalse };
    // The split view input is open over the lower part of the screen.
    TBool iSplitInput { EFalse };

    TPoint iPointerDown;
    CAsyncCallBack* iSelfTest { nullptr };
};
