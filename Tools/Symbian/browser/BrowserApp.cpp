// The browser application: an AVKON app built without a resource file. The
// softkeys, the menus (popup lists) and the address field (an edwin) are
// created in code.

#include "NativeDeclarations.h"
#include "RunLoopDriver.h"
#include "WebContainer.h"

#include <aknapp.h>
#include <aknappui.h>
#include <akndoc.h>
#include <aknlists.h>
#include <aknPopup.h>
#include <badesca.h>
#include <avkon.hrh>
#include <eikbtgpc.h>
#include <eikedwin.h>
#include <eikenv.h>
#include <gulcolor.h>

using namespace WebKitSymbian;

namespace {

const TUid KUidBrowser = { static_cast<TInt32>(0xE1A5C002) };
// The engine keeps cookies and caches here.
const char KDataDirectory[] = "C:/Data/WebKit";
const TInt KUrlBarHeight = 48;
const TInt KUrlMaxLength = 2048;

enum TBrowserCommand {
    ECmdOptions = 0x6000,
    ECmdGoTo,
    ECmdBack,
    ECmdForward,
    ECmdReload,
    ECmdStop,
    ECmdExit,
};

// Shows a list of items in an AVKON popup and waits for a choice. Returns
// the chosen index, or -1 if the list was cancelled.
TInt ShowPopupListL(const TDesC& aTitle, const MDesCArray& aItems, TInt aSelected)
{
    CAknSinglePopupMenuStyleListBox* list = new (ELeave) CAknSinglePopupMenuStyleListBox;
    CleanupStack::PushL(list);
    // Softkeys are set below: there is no resource file for a CBA.
    CAknPopupList* popup = CAknPopupList::NewL(list, 0, AknPopupLayouts::EMenuWindow);
    CleanupStack::PushL(popup);
    list->ConstructL(popup, CEikListBox::ELeftDownInViewRect);
    list->CreateScrollBarFrameL(ETrue);
    list->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff, CEikScrollBarFrame::EAuto);
    CDesCArrayFlat* items = new (ELeave) CDesCArrayFlat(Max(aItems.MdcaCount(), 1));
    CleanupStack::PushL(items);
    for (TInt i = 0; i < aItems.MdcaCount(); ++i)
        items->AppendL(aItems.MdcaPoint(i));
    list->Model()->SetItemTextArray(items);
    list->Model()->SetOwnershipType(ELbmOwnsItemArray);
    CleanupStack::Pop(items);
    if (aSelected >= 0 && aSelected < aItems.MdcaCount())
        list->SetCurrentItemIndex(aSelected);
    if (aTitle.Length())
        popup->SetTitleL(aTitle);
    CEikButtonGroupContainer* softkeys = popup->ButtonGroupContainer();
    softkeys->SetCommandL(CEikButtonGroupContainer::ELeftSoftkeyPosition, EAknSoftkeySelect, _L("Select"));
    softkeys->SetCommandL(CEikButtonGroupContainer::ERightSoftkeyPosition, EAknSoftkeyCancel, _L("Cancel"));
    CleanupStack::Pop(popup);
    TBool chosen = popup->ExecuteLD();
    TInt index = chosen ? list->CurrentItemIndex() : -1;
    CleanupStack::PopAndDestroy(list);
    return index;
}

class CBrowserAppUi;

// The address field above the page: an edwin over a plain background, with
// the load progress as a line along its bottom edge.
class CUrlBar : public CCoeControl, public MCoeControlObserver {
public:
    CUrlBar(CBrowserAppUi& aAppUi)
        : iAppUi(aAppUi)
    {
    }

    ~CUrlBar() { delete iEdwin; }

    void ConstructL(const TRect& aRect)
    {
        CreateWindowL();
        iEdwin = new (ELeave) CEikEdwin;
        iEdwin->SetContainerWindowL(*this);
        iEdwin->ConstructL(EEikEdwinNoWrap | EEikEdwinNoLineOrParaBreaks | EEikEdwinAutoSelection, 0, KUrlMaxLength, 1);
        iEdwin->SetObserver(this);
        SetRect(aRect);
        ActivateL();
    }

    void SetUrlL(const TDesC& aUrl)
    {
        if (!iEdwin->IsFocused())
            iEdwin->SetTextL(&aUrl);
        iEdwin->DrawDeferred();
    }

    void SetProgress(TBool aLoading, TInt aPercent)
    {
        iProgress = aLoading ? aPercent : 0;
        DrawDeferred();
    }

    void StartEditingL()
    {
        SetFocus(ETrue);
        iEdwin->SelectAllL();
    }

    TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);

private:
    TInt CountComponentControls() const { return 1; }
    CCoeControl* ComponentControl(TInt) const { return iEdwin; }

    void SizeChanged()
    {
        TRect rect = Rect();
        rect.Shrink(6, 8);
        iEdwin->SetRect(rect);
    }

    void FocusChanged(TDrawNow aDrawNow) { iEdwin->SetFocus(IsFocused(), aDrawNow); }

    void Draw(const TRect&) const
    {
        CWindowGc& gc = SystemGc();
        gc.SetPenStyle(CGraphicsContext::ENullPen);
        gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
        gc.SetBrushColor(TRgb(0xe8, 0xe8, 0xe8));
        gc.DrawRect(Rect());
        if (iProgress > 0) {
            TRect bar = Rect();
            bar.iTl.iY = bar.iBr.iY - 3;
            bar.SetWidth(bar.Width() * iProgress / 100);
            gc.SetBrushColor(TRgb(0x30, 0x7c, 0xf0));
            gc.DrawRect(bar);
        }
    }

    void HandlePointerEventL(const TPointerEvent& aPointerEvent);
    void HandleControlEventL(CCoeControl*, TCoeEvent) { }

    CBrowserAppUi& iAppUi;
    CEikEdwin* iEdwin { nullptr };
    TInt iProgress { 0 };
};

class CBrowserAppUi : public CAknAppUi, public MWebContainerObserver {
public:
    void ConstructL();
    ~CBrowserAppUi();

    void LoadUrlL(const TDesC& aUrl);
    void FocusUrlBarL();
    void FocusPageL();

private:
    void HandleCommandL(TInt aCommand);
    void HandleResourceChangeL(TInt aType);
    void LayoutControls();
    void ShowOptionsMenuL();

    // MWebContainerObserver
    void WebTitleChanged(const TDesC& aTitle);
    void WebUrlChanged(const TDesC& aUrl);
    void WebProgressChanged(TBool aLoading, TInt aPercent);
    TInt WebSelectPopupL(const Text* aItems, TInt aCount, TInt aSelected);

    CRunLoopDriver* iRunLoop { nullptr };
    CEikButtonGroupContainer* iSoftkeys { nullptr };
    CUrlBar* iUrlBar { nullptr };
    CWebContainer* iWebContainer { nullptr };
    TBool iLoading { EFalse };
    TBool iShowingPopup { EFalse };
};

TKeyResponse CUrlBar::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
{
    if (aType == EEventKey && (aKeyEvent.iCode == EKeyEnter || aKeyEvent.iCode == EKeyOK || aKeyEvent.iCode == EKeyDevice3)) {
        HBufC* url = iEdwin->GetTextInHBufL();
        if (url) {
            CleanupStack::PushL(url);
            iAppUi.LoadUrlL(*url);
            CleanupStack::PopAndDestroy(url);
        }
        iAppUi.FocusPageL();
        return EKeyWasConsumed;
    }
    return iEdwin->OfferKeyEventL(aKeyEvent, aType);
}

void CUrlBar::HandlePointerEventL(const TPointerEvent& aPointerEvent)
{
    if (aPointerEvent.iType == TPointerEvent::EButton1Down && !IsFocused())
        iAppUi.FocusUrlBarL();
    CCoeControl::HandlePointerEventL(aPointerEvent);
}

void CBrowserAppUi::ConstructL()
{
    BaseConstructL(EAknEnableSkin | ENoAppResourceFile | EAknTouchCompatible | EAknSingleClickCompatible);

    WebView::initializeProcess(KDataDirectory);
    iRunLoop = CRunLoopDriver::NewL();

    iSoftkeys = CEikButtonGroupContainer::NewL(CEikButtonGroupContainer::ECba,
        CEikButtonGroupContainer::EHorizontal, this, 0);
    iSoftkeys->SetCommandL(CEikButtonGroupContainer::ELeftSoftkeyPosition, ECmdOptions, _L("Options"));
    iSoftkeys->SetCommandL(CEikButtonGroupContainer::ERightSoftkeyPosition, ECmdBack, _L("Back"));

    iUrlBar = new (ELeave) CUrlBar(*this);
    iUrlBar->ConstructL(TRect(0, 0, 1, 1));
    iWebContainer = CWebContainer::NewL(TRect(0, 0, 1, 1), *this);
    LayoutControls();
    AddToStackL(iUrlBar);
    AddToStackL(iWebContainer);
    iSoftkeys->MakeVisible(ETrue);
    iSoftkeys->DrawNow();
    FocusPageL();

    // The command line may name a URL to load at start, and fep-selftest.
    HBufC* commandLine = HBufC::NewLC(User::CommandLineLength());
    TPtr commandLinePtr = commandLine->Des();
    User::CommandLine(commandLinePtr);
    TLex lex(commandLinePtr);
    for (TPtrC token = lex.NextToken(); token.Length(); token.Set(lex.NextToken())) {
        if (token == _L("fep-selftest"))
            iWebContainer->EnableInputSelfTest();
        else
            LoadUrlL(token);
    }
    CleanupStack::PopAndDestroy(commandLine);
}

CBrowserAppUi::~CBrowserAppUi()
{
    if (iWebContainer)
        RemoveFromStack(iWebContainer);
    if (iUrlBar)
        RemoveFromStack(iUrlBar);
    delete iWebContainer;
    delete iUrlBar;
    delete iSoftkeys;
    delete iRunLoop;
}

void CBrowserAppUi::LayoutControls()
{
    TRect rect = ApplicationRect();
    iSoftkeys->SetBoundingRect(rect);
    iSoftkeys->ReduceRect(rect);
    TRect urlRect = rect;
    urlRect.SetHeight(KUrlBarHeight);
    rect.iTl.iY = urlRect.iBr.iY;
    iUrlBar->SetRect(urlRect);
    iWebContainer->SetRect(rect);
}

void CBrowserAppUi::HandleResourceChangeL(TInt aType)
{
    CAknAppUi::HandleResourceChangeL(aType);
    if (aType == KEikDynamicLayoutVariantSwitch)
        LayoutControls();
}

void CBrowserAppUi::LoadUrlL(const TDesC& aUrl)
{
    HBufC8* utf8 = HBufC8::NewLC(aUrl.Length() * 3 + 1);
    TPtr8 utf8Ptr = utf8->Des();
    for (TInt i = 0; i < aUrl.Length(); ++i) {
        TUint c = aUrl[i];
        if (c < 0x80)
            utf8Ptr.Append(c);
        else if (c < 0x800) {
            utf8Ptr.Append(0xC0 | (c >> 6));
            utf8Ptr.Append(0x80 | (c & 0x3F));
        } else {
            utf8Ptr.Append(0xE0 | (c >> 12));
            utf8Ptr.Append(0x80 | ((c >> 6) & 0x3F));
            utf8Ptr.Append(0x80 | (c & 0x3F));
        }
    }
    iWebContainer->WebView().load(reinterpret_cast<const char*>(utf8Ptr.PtrZ()));
    CleanupStack::PopAndDestroy(utf8);
}

void CBrowserAppUi::FocusUrlBarL()
{
    iWebContainer->SetFocus(EFalse);
    iUrlBar->StartEditingL();
}

void CBrowserAppUi::FocusPageL()
{
    iUrlBar->SetFocus(EFalse);
    iWebContainer->SetFocus(ETrue);
}

void CBrowserAppUi::HandleCommandL(TInt aCommand)
{
    WebView& webView = iWebContainer->WebView();
    switch (aCommand) {
    case ECmdOptions:
        ShowOptionsMenuL();
        break;
    case ECmdGoTo:
        FocusUrlBarL();
        break;
    case ECmdBack:
        if (webView.canGoBack())
            webView.goBack();
        break;
    case ECmdForward:
        webView.goForward();
        break;
    case ECmdReload:
        webView.reload();
        break;
    case ECmdStop:
        webView.stop();
        break;
    case ECmdExit:
    case EAknSoftkeyExit:
    case EEikCmdExit:
        Exit();
        break;
    default:
        break;
    }
}

void CBrowserAppUi::ShowOptionsMenuL()
{
    WebView& webView = iWebContainer->WebView();
    RArray<TInt> commands;
    CleanupClosePushL(commands);
    CDesCArrayFlat* items = new (ELeave) CDesCArrayFlat(5);
    CleanupStack::PushL(items);
    items->AppendL(_L("Go to address"));
    commands.AppendL(ECmdGoTo);
    if (webView.canGoForward()) {
        items->AppendL(_L("Forward"));
        commands.AppendL(ECmdForward);
    }
    items->AppendL(iLoading ? _L("Stop") : _L("Reload"));
    commands.AppendL(iLoading ? ECmdStop : ECmdReload);
    items->AppendL(_L("Exit"));
    commands.AppendL(ECmdExit);
    TInt index = ShowPopupListL(KNullDesC, *items, 0);
    iSoftkeys->DrawNow();
    TInt command = index >= 0 ? commands[index] : 0;
    CleanupStack::PopAndDestroy(2, &commands);
    if (command)
        HandleCommandL(command);
}

void CBrowserAppUi::WebTitleChanged(const TDesC&)
{
}

void CBrowserAppUi::WebUrlChanged(const TDesC& aUrl)
{
    iUrlBar->SetUrlL(aUrl);
}

void CBrowserAppUi::WebProgressChanged(TBool aLoading, TInt aPercent)
{
    iLoading = aLoading;
    iUrlBar->SetProgress(aLoading, aPercent);
}

TInt CBrowserAppUi::WebSelectPopupL(const Text* aItems, TInt aCount, TInt aSelected)
{
    if (iShowingPopup || aCount <= 0)
        return -1;
    CDesCArrayFlat* items = new (ELeave) CDesCArrayFlat(aCount);
    CleanupStack::PushL(items);
    for (TInt i = 0; i < aCount; ++i)
        items->AppendL(TPtrC(reinterpret_cast<const TText*>(aItems[i].characters), aItems[i].length));
    iShowingPopup = ETrue;
    TInt index = -1;
    TRAPD(error, index = ShowPopupListL(KNullDesC, *items, aSelected));
    iShowingPopup = EFalse;
    iSoftkeys->DrawNow();
    CleanupStack::PopAndDestroy(items);
    User::LeaveIfError(error);
    return index;
}

class CBrowserDocument : public CAknDocument {
public:
    CBrowserDocument(CEikApplication& aApp)
        : CAknDocument(aApp)
    {
    }

private:
    CEikAppUi* CreateAppUiL() { return new (ELeave) CBrowserAppUi; }
};

class CBrowserApplication : public CAknApplication {
private:
    TUid AppDllUid() const { return KUidBrowser; }
    CApaDocument* CreateDocumentL() { return new (ELeave) CBrowserDocument(*this); }
    // No resource file: everything is built in code.
    TFileName ResourceFileName() const { return KNullDesC(); }
};

CApaApplication* NewApplication()
{
    return new CBrowserApplication;
}

} // namespace

int main()
{
    return EikStart::RunApplication(NewApplication);
}
