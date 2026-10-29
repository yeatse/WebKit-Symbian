// The browser application: an AVKON app built without a resource file. The
// softkeys, the menus (popup lists) and the address field (an edwin) are
// created in code.

#include "BrowserLog.h"
#include "NativeDeclarations.h"
#include "NativeHeapDelete.h"
#include "NetworkConnection.h"
#include "PageList.h"
#include "RunLoopDriver.h"
#include "WebContainer.h"

#include <aknapp.h>
#include <aknappui.h>
#include <akndoc.h>
#include <aknlists.h>
#include <aknPopup.h>
#include <badesca.h>
#include <bautils.h>
#include <stdio.h>
#include <stdlib.h>
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
const char KHistoryFile[] = "C:/Data/WebKit/history.txt";
const char KBookmarksFile[] = "C:/Data/WebKit/bookmarks.txt";
const TInt KHistoryMaxCount = 100;
const TInt KUrlBarHeight = 48;
const TInt KSuggestionMargin = 6;
const TInt KSuggestionMaxCount = 6;
const TInt KUrlMaxLength = 2048;

enum TBrowserCommand {
    ECmdOptions = 0x6000,
    ECmdGoTo,
    ECmdBookmarks,
    ECmdAddBookmark,
    ECmdRemoveBookmark,
    ECmdHistory,
    ECmdBack,
    ECmdForward,
    ECmdReload,
    ECmdStop,
    ECmdExit,
};

// Whether a URL needs the network connection: anything but a local file or
// a page the engine makes itself.
TBool IsNetworkUrl(const TDesC& aUrl)
{
    if (aUrl.Length() >= 2 && aUrl[1] == ':' && TChar(aUrl[0]).IsAlpha())
        return EFalse; // A drive path, C:/Data/...
    _LIT(KFile, "file:");
    _LIT(KAbout, "about:");
    _LIT(KData, "data:");
    _LIT(KJavaScript, "javascript:");
    const TDesC* local[] = { &KFile, &KAbout, &KData, &KJavaScript };
    for (const TDesC* scheme : local) {
        if (aUrl.Left(scheme->Length()).CompareF(*scheme) == 0)
            return EFalse;
    }
    return ETrue;
}

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
class CUrlBar : public CCoeControl, public MCoeControlObserver, public MEikEdwinObserver {
public:
    SYMBIAN_BROWSER_NATIVE_DELETE
    CUrlBar(CBrowserAppUi& aAppUi)
        : iAppUi(aAppUi)
    {
    }

    ~CUrlBar()
    {
        PrepareForExit();
        delete iEdwin;
        User::Free(iText);
    }

    void PrepareForExit()
    {
        iClosing = ETrue;
        if (iEdwin) {
            iEdwin->SetObserver(nullptr);
            iEdwin->SetEdwinObserver(nullptr);
        }
    }

    void ConstructL(const TRect& aRect)
    {
        CreateWindowL();
        iEdwin = new (ELeave) CEikEdwin;
        iEdwin->SetContainerWindowL(*this);
        iEdwin->ConstructL(EEikEdwinNoWrap | EEikEdwinNoLineOrParaBreaks | EEikEdwinAutoSelection, 0, KUrlMaxLength, 1);
        iEdwin->SetObserver(this);
        iEdwin->SetEdwinObserver(this);
        // The touch keyboard over the lower part of the screen, as for the
        // page's fields, and addresses start in lower case.
        iEdwin->SetAknEditorFlags(EAknEditorFlagNoT9 | EAknEditorFlagEnablePartialScreen | EAknEditorFlagSelectionVisible);
        iEdwin->SetAknEditorCase(EAknEditorLowerCase);
        iEdwin->SetAknEditorPermittedCaseModes(EAknEditorAllCaseModes);
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

    // Takes the focus with the address selected, so typing replaces it.
    void StartEditingL();

    TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);

private:
    // What the suggestions are filtered by: the text typed, or nothing
    // while the address shown is still all selected.
    TPtrC FilterL();
    void SuggestL();

    TInt CountComponentControls() const { return 1; }
    CCoeControl* ComponentControl(TInt) const { return iEdwin; }

    void SizeChanged()
    {
        TRect rect = Rect();
        rect.Shrink(6, 8);
        iEdwin->SetRect(rect);
    }

    void FocusChanged(TDrawNow aDrawNow);

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
    void HandleEdwinEventL(CEikEdwin*, TEdwinEvent aEventType);

    CBrowserAppUi& iAppUi;
    CEikEdwin* iEdwin { nullptr };
    HBufC* iText { nullptr };
    TInt iProgress { 0 };
    TBool iClosing { EFalse };
};

// Pages from the bookmarks and the history that match what is typed in the
// address bar, in a list under it; a tap loads one. It never takes the
// focus, so typing goes on in the address bar.
class CUrlSuggestions : public CCoeControl {
public:
    SYMBIAN_BROWSER_NATIVE_DELETE
    CUrlSuggestions(CBrowserAppUi& aAppUi)
        : iAppUi(aAppUi)
    {
    }

    ~CUrlSuggestions();

    void ConstructL()
    {
        CreateWindowL();
        EnableDragEvents();
        MakeVisible(EFalse);
        ActivateL();
    }

    // Bookmarks first, then the most recent pages, up to aMaxCount. An
    // empty filter matches every page.
    void SetPagesL(const CPageList& aBookmarks, const CPageList& aHistory, const TDesC& aFilter, TInt aMaxCount);
    TInt Count() const { return iUrls.Count(); }
    // A row has the title over the address.
    TInt RowHeight() const;

private:
    const CFont* TitleFont() const { return iEikonEnv->NormalFont(); }
    const CFont* UrlFont() const { return iEikonEnv->AnnotationFont(); }
    void AddPageL(const CPageList& aPages, TInt aIndex, const TDesC& aFilter);
    void Draw(const TRect&) const;
    void HandlePointerEventL(const TPointerEvent& aPointerEvent);

    CBrowserAppUi& iAppUi;
    RPointerArray<HBufC> iUrls;
    RPointerArray<HBufC> iLabels;
    // The row under the finger, drawn highlighted.
    TInt iPressed { -1 };
};

class CBrowserAppUi : public CAknAppUi, public MWebContainerObserver, public MNetworkConnectionObserver {
public:
    SYMBIAN_BROWSER_NATIVE_DELETE
    void ConstructL() override;
    ~CBrowserAppUi() override;

    void LoadUrlL(const TDesC& aUrl);
    void FocusUrlBarL();
    void FocusPageL();
    // Shows the pages matching aFilter under the address bar, or hides
    // them for a null filter.
    void SuggestL(const TDesC* aFilter);

private:
    void PrepareToExit() override;
    void BeginTeardown();
    void HandleCommandL(TInt aCommand) override;
    void HandleResourceChangeL(TInt aType) override;
    void LayoutControls();
    void ShowOptionsMenuL();
    // Lists the pages and loads the chosen one. aExtraItem, if any, is
    // shown last; returns ETrue if it was chosen.
    TBool ShowPageListL(const TDesC& aTitle, const CPageList& aPages, const TDesC& aExtraItem);

    // MWebContainerObserver
    void WebTitleChanged(const TDesC& aTitle) override;
    void WebUrlChanged(const TDesC& aUrl) override;
    void WebProgressChanged(TBool aLoading, TInt aPercent) override;
    TInt WebSelectPopupL(const Text* aItems, TInt aCount, TInt aSelected) override;
    void WebPointerDownL() override;

    // MNetworkConnectionObserver
    void NetworkConnectionReady(TInt aError) override;

    void LoadNowL(const TDesC& aUrl);
    // Sets the softkeys again: a popup list leaves Options hidden.
    void SetSoftkeysL();
    void StartHeartbeatL();
    static TInt Heartbeat(TAny* aSelf);

    CRunLoopDriver* iRunLoop { nullptr };
    CNetworkConnection* iNetwork { nullptr };
    // A network URL that waits for the connection.
    HBufC* iPendingUrl { nullptr };
    CEikButtonGroupContainer* iSoftkeys { nullptr };
    CUrlBar* iUrlBar { nullptr };
    CWebContainer* iWebContainer { nullptr };
    CUrlSuggestions* iSuggestions { nullptr };
    CPageList* iHistory { nullptr };
    CPageList* iBookmarks { nullptr };
    // The page shown, for the history and the bookmarks.
    HBufC* iUrl { nullptr };
    HBufC* iTitle { nullptr };
    TBool iLoading { EFalse };
    TInt iProgress { 0 };
    // With curl's trace on, logs that the UI thread still runs.
    CPeriodic* iHeartbeat { nullptr };
    TBool iShowingPopup { EFalse };
    // The split view input is open; the page ends above it.
    TBool iSplitInput { EFalse };
    TBool iClosing { EFalse };
    TBool iPreparedForExit { EFalse };
    TBool iUrlBarOnStack { EFalse };
    TBool iWebContainerOnStack { EFalse };
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

TPtrC CUrlBar::FilterL()
{
    User::Free(iText);
    iText = nullptr;
    iText = HBufC::NewL(iEdwin->TextLength());
    TPtr text = iText->Des();
    iEdwin->GetText(text);
    text.Trim();
    if (iEdwin->SelectionLength() == iEdwin->TextLength())
        return TPtrC();
    return text;
}

void CUrlBar::StartEditingL()
{
    SetFocus(ETrue);
    iEdwin->SelectAllL();
    SuggestL();
}

void CUrlBar::SuggestL()
{
    TPtrC filter = FilterL();
    iAppUi.SuggestL(&filter);
}

void CUrlBar::FocusChanged(TDrawNow aDrawNow)
{
    if (iClosing)
        return;
    iEdwin->SetFocus(IsFocused(), aDrawNow);
    if (!IsFocused())
        TRAP_IGNORE(iAppUi.SuggestL(nullptr));
}

void CUrlBar::HandleEdwinEventL(CEikEdwin*, TEdwinEvent aEventType)
{
    if (!iClosing && aEventType == EEventTextUpdate && IsFocused())
        SuggestL();
}

CUrlSuggestions::~CUrlSuggestions()
{
    for (TInt i = 0; i < iUrls.Count(); ++i) {
        User::Free(iUrls[i]);
        User::Free(iLabels[i]);
    }
    iUrls.Close();
    iLabels.Close();
}

void CUrlSuggestions::SetPagesL(const CPageList& aBookmarks, const CPageList& aHistory, const TDesC& aFilter, TInt aMaxCount)
{
    while (iUrls.Count()) {
        TInt last = iUrls.Count() - 1;
        User::Free(iUrls[last]);
        User::Free(iLabels[last]);
        iUrls.Remove(last);
        iLabels.Remove(last);
    }
    iPressed = -1;
    for (TInt i = 0; i < aBookmarks.Count() && iUrls.Count() < aMaxCount; ++i)
        AddPageL(aBookmarks, i, aFilter);
    for (TInt i = 0; i < aHistory.Count() && iUrls.Count() < aMaxCount; ++i)
        AddPageL(aHistory, i, aFilter);
}

void CUrlSuggestions::AddPageL(const CPageList& aPages, TInt aIndex, const TDesC& aFilter)
{
    const TDesC& url = aPages.Url(aIndex);
    const TDesC& label = aPages.Label(aIndex);
    if (aFilter.Length() && url.FindF(aFilter) == KErrNotFound && label.FindF(aFilter) == KErrNotFound)
        return;
    for (TInt i = 0; i < iUrls.Count(); ++i) {
        if (*iUrls[i] == url)
            return;
    }
    HBufC* urlCopy = url.AllocLC();
    HBufC* labelCopy = label.AllocLC();
    iUrls.AppendL(urlCopy);
    TRAPD(error, iLabels.AppendL(labelCopy));
    if (error) {
        iUrls.Remove(iUrls.Count() - 1);
        User::Leave(error);
    }
    CleanupStack::Pop(2, urlCopy);
}

TInt CUrlSuggestions::RowHeight() const
{
    return TitleFont()->HeightInPixels() + UrlFont()->HeightInPixels() + 3 * KSuggestionMargin;
}

void CUrlSuggestions::Draw(const TRect&) const
{
    CWindowGc& gc = SystemGc();
    gc.SetPenStyle(CGraphicsContext::ENullPen);
    gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
    gc.SetBrushColor(KRgbWhite);
    gc.DrawRect(Rect());
    const CFont* titleFont = TitleFont();
    const CFont* urlFont = UrlFont();
    const TInt rowHeight = RowHeight();
    for (TInt i = 0; i < iUrls.Count(); ++i) {
        TRect row(Rect().iTl + TPoint(0, i * rowHeight), TSize(Rect().Width(), rowHeight));
        if (i == iPressed) {
            gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
            gc.SetBrushColor(TRgb(0xd0, 0xe0, 0xfc));
            gc.SetPenStyle(CGraphicsContext::ENullPen);
            gc.DrawRect(row);
        }
        gc.SetBrushStyle(CGraphicsContext::ENullBrush);
        gc.SetPenStyle(CGraphicsContext::ESolidPen);
        // The title, and the address under it in grey.
        TRect title(row.iTl, TSize(row.Width(), KSuggestionMargin + titleFont->HeightInPixels()));
        gc.UseFont(titleFont);
        gc.SetPenColor(KRgbBlack);
        gc.DrawText(*iLabels[i], title, KSuggestionMargin + titleFont->AscentInPixels(), CGraphicsContext::ELeft, 2 * KSuggestionMargin);
        TRect url(TPoint(row.iTl.iX, title.iBr.iY), row.iBr);
        gc.UseFont(urlFont);
        gc.SetPenColor(TRgb(0x60, 0x60, 0x60));
        gc.DrawText(*iUrls[i], url, KSuggestionMargin + urlFont->AscentInPixels(), CGraphicsContext::ELeft, 2 * KSuggestionMargin);
        gc.SetPenColor(TRgb(0xd8, 0xd8, 0xd8));
        gc.DrawLine(TPoint(row.iTl.iX, row.iBr.iY - 1), TPoint(row.iBr.iX, row.iBr.iY - 1));
    }
    gc.DiscardFont();
}

void CUrlSuggestions::HandlePointerEventL(const TPointerEvent& aPointerEvent)
{
    TInt row = (aPointerEvent.iPosition.iY - Rect().iTl.iY) / RowHeight();
    if (!Rect().Contains(aPointerEvent.iPosition) || row >= iUrls.Count())
        row = -1;
    if (aPointerEvent.iType == TPointerEvent::EButton1Down || aPointerEvent.iType == TPointerEvent::EDrag) {
        if (row != iPressed) {
            iPressed = row;
            DrawNow();
        }
    } else if (aPointerEvent.iType == TPointerEvent::EButton1Up) {
        iPressed = -1;
        if (row < 0) {
            DrawNow();
            return;
        }
        // Loading changes the history, and with it the list.
        HBufC* url = iUrls[row]->AllocLC();
        iAppUi.FocusPageL();
        iAppUi.LoadUrlL(*url);
        CleanupStack::PopAndDestroy(url);
    }
}

void CBrowserAppUi::ConstructL()
{
    BaseConstructL(EAknEnableSkin | ENoAppResourceFile | EAknTouchCompatible | EAknSingleClickCompatible);

    // curl's own trace goes to C:\Data\WebKit\curl.txt while
    // C:\Data\WebKit\curl-verbose exists, or with curl-verbose on the command
    // line. The engine reads the settings when it starts.
    TBool curlTrace = BaflUtils::FileExists(iEikonEnv->FsSession(), _L("C:\\Data\\WebKit\\curl-verbose"));
    HBufC* commandLine = HBufC::NewLC(User::CommandLineLength());
    TPtr commandLinePtr = commandLine->Des();
    User::CommandLine(commandLinePtr);
    if (commandLinePtr.Find(_L("curl-verbose")) != KErrNotFound)
        curlTrace = ETrue;
    if (curlTrace) {
        remove("C:/Data/WebKit/curl.txt");
        setenv("DEBUG_CURL", "1", 1);
        setenv("CURL_LOG_FILE", "C:/Data/WebKit/curl.txt", 1);
    }
    WebView::initializeProcess(KDataDirectory);
    BrowserLogOpen(KDataDirectory);
    BrowserLog("WebKit Browser starting");
    if (curlTrace) {
        StartHeartbeatL();
        BrowserThreadWatchStart();
    }
    iRunLoop = CRunLoopDriver::NewL();
    iHistory = CPageList::NewL(KHistoryFile, KHistoryMaxCount);
    iBookmarks = CPageList::NewL(KBookmarksFile, 0);
    // Start the connection now, so the system's access point prompt (if
    // any) comes up once, while the first page is being typed.
    iNetwork = CNetworkConnection::NewL(*this);
    TRAPD(error, iNetwork->StartL());
    if (error)
        BrowserLog("network: cannot start (%d)", error);

    iSoftkeys = CEikButtonGroupContainer::NewL(CEikButtonGroupContainer::ECba,
        CEikButtonGroupContainer::EHorizontal, this, 0);
    SetSoftkeysL();

    iUrlBar = new (ELeave) CUrlBar(*this);
    iUrlBar->ConstructL(TRect(0, 0, 1, 1));
    iWebContainer = CWebContainer::NewL(TRect(0, 0, 1, 1), *this);
    // Over the page: a window made later is in front.
    iSuggestions = new (ELeave) CUrlSuggestions(*this);
    iSuggestions->ConstructL();
    LayoutControls();
    // RemoveFromStack tolerates a failed insertion. Mark before AddToStackL,
    // since the insertion's focus notification can itself leave.
    iUrlBarOnStack = ETrue;
    AddToStackL(iUrlBar);
    iWebContainerOnStack = ETrue;
    AddToStackL(iWebContainer);
    iSoftkeys->MakeVisible(ETrue);
    iSoftkeys->DrawNow();
    FocusPageL();

    // The command line may name a URL to load at start, fep-selftest and
    // curl-verbose.
    TLex lex(commandLinePtr);
    for (TPtrC token = lex.NextToken(); token.Length(); token.Set(lex.NextToken())) {
        if (token == _L("fep-selftest"))
            iWebContainer->EnableInputSelfTest();
        else if (token != _L("curl-verbose"))
            LoadUrlL(token);
    }
    CleanupStack::PopAndDestroy(commandLine);
}

CBrowserAppUi::~CBrowserAppUi()
{
    BrowserLog("exit diagnostic: appui destructor begin");
    BeginTeardown();
    BrowserLog("exit diagnostic: delete suggestions");
    delete iSuggestions;
    BrowserLog("exit diagnostic: delete web container");
    delete iWebContainer;
    BrowserLog("exit diagnostic: delete url bar");
    delete iUrlBar;
    BrowserLog("exit diagnostic: delete softkeys");
    delete iSoftkeys;
    BrowserLog("exit diagnostic: delete run loop");
    delete iRunLoop;
    BrowserLog("exit diagnostic: delete remaining app state");
    User::Free(iPendingUrl);
    delete iHistory;
    delete iBookmarks;
    User::Free(iUrl);
    User::Free(iTitle);
    delete iNetwork;
    delete iHeartbeat;
    BrowserLog("WebKit Browser exiting");
}

void CBrowserAppUi::PrepareToExit()
{
    if (iPreparedForExit)
        return;
    iPreparedForExit = ETrue;
    BrowserLog("exit: prepare appui while native environment is alive");
    BeginTeardown();
    CAknAppUi::PrepareToExit();
}

void CBrowserAppUi::BeginTeardown()
{
    if (iClosing)
        return;
    iClosing = ETrue;
    // Stop engine delivery before deleting clients. The fallback call from
    // our destructor also handles a leave during ConstructL.
    delete iRunLoop;
    iRunLoop = nullptr;
    if (iHeartbeat)
        iHeartbeat->Cancel();
    if (iNetwork)
        iNetwork->Cancel();
    if (iWebContainer)
        iWebContainer->PrepareForExit();
    if (iUrlBar)
        iUrlBar->PrepareForExit();

    // CEikonEnv releases its font array before deleting the AppUi. Removing
    // the page from the stack can focus the URL editor and ask for fonts, so
    // normally do this in PrepareToExit, before that environment teardown.
    // Closing guards also suppress those callbacks on partial construction.
    if (iWebContainerOnStack) {
        iWebContainerOnStack = EFalse;
        RemoveFromStack(iWebContainer);
    }
    if (iUrlBarOnStack) {
        iUrlBarOnStack = EFalse;
        RemoveFromStack(iUrlBar);
    }
}

void CBrowserAppUi::SetSoftkeysL()
{
    iSoftkeys->SetCommandL(CEikButtonGroupContainer::ELeftSoftkeyPosition, ECmdOptions, _L("Options"));
    iSoftkeys->SetCommandL(CEikButtonGroupContainer::ERightSoftkeyPosition, ECmdBack, _L("Back"));
    iSoftkeys->MakeCommandVisible(ECmdOptions, ETrue);
    iSoftkeys->MakeCommandVisible(ECmdBack, ETrue);
    iSoftkeys->DrawNow();
}

void CBrowserAppUi::StartHeartbeatL()
{
    iHeartbeat = CPeriodic::NewL(CActive::EPriorityStandard);
    const TInt interval = 2000000;
    iHeartbeat->Start(interval, interval, TCallBack(Heartbeat, this));
    BrowserLog("curl trace on, in curl.txt");
}

TInt CBrowserAppUi::Heartbeat(TAny* aSelf)
{
    auto* self = static_cast<CBrowserAppUi*>(aSelf);
    if (self->iLoading)
        BrowserLog("UI thread running, loading %d%%", self->iProgress);
    else
        BrowserLog("UI thread running");
    return ETrue;
}

void CBrowserAppUi::LayoutControls()
{
    if (iClosing)
        return;
    TRect rect = ApplicationRect();
    iSoftkeys->SetBoundingRect(rect);
    iSoftkeys->ReduceRect(rect);
    TRect urlRect = rect;
    urlRect.SetHeight(KUrlBarHeight);
    rect.iTl.iY = urlRect.iBr.iY;
    if (iSplitInput) {
        // While the split view input is open, the client rectangle is the
        // part of the screen above it.
        TInt visibleBottom = ClientRect().iBr.iY;
        if (visibleBottom > rect.iTl.iY)
            rect.iBr.iY = Min(rect.iBr.iY, visibleBottom);
    }
    iUrlBar->SetRect(urlRect);
    iWebContainer->SetRect(rect);
    TRect suggestionsRect = rect;
    suggestionsRect.SetHeight(Min(rect.Height(), iSuggestions->Count() * iSuggestions->RowHeight()));
    iSuggestions->SetRect(suggestionsRect);
}

void CBrowserAppUi::SuggestL(const TDesC* aFilter)
{
    if (iClosing)
        return;
    if (aFilter) {
        // As many rows as fit above the split view input, if it is open.
        TInt fit = iWebContainer->Rect().Height() / iSuggestions->RowHeight();
        iSuggestions->SetPagesL(*iBookmarks, *iHistory, *aFilter, Min(fit, KSuggestionMaxCount));
    }
    TBool visible = aFilter && iSuggestions->Count();
    if (visible)
        LayoutControls();
    if (visible != iSuggestions->IsVisible())
        iSuggestions->MakeVisible(visible);
    if (visible)
        iSuggestions->DrawNow();
}

void CBrowserAppUi::HandleResourceChangeL(TInt aType)
{
    if (iClosing)
        return;
    CAknAppUi::HandleResourceChangeL(aType);
    if (aType == KAknSplitInputEnabled || aType == KAknSplitInputDisabled) {
        iSplitInput = aType == KAknSplitInputEnabled;
        BrowserLog("split view input %s, client rect %d,%d-%d,%d", iSplitInput ? "open" : "closed",
            ClientRect().iTl.iX, ClientRect().iTl.iY, ClientRect().iBr.iX, ClientRect().iBr.iY);
        LayoutControls();
    } else if (aType == KEikDynamicLayoutVariantSwitch)
        LayoutControls();
}

void CBrowserAppUi::LoadUrlL(const TDesC& aUrl)
{
    if (!IsNetworkUrl(aUrl) || iNetwork->IsReady()) {
        LoadNowL(aUrl);
        return;
    }
    // Load once the connection is up, or has failed (the page then shows
    // the error).
    HBufC* url = aUrl.AllocL();
    User::Free(iPendingUrl);
    iPendingUrl = url;
    iUrlBar->SetUrlL(aUrl);
    iUrlBar->SetProgress(ETrue, 5);
    iNetwork->StartL();
}

void CBrowserAppUi::LoadNowL(const TDesC& aUrl)
{
    HBufC8* utf8 = ToUtf8LC(aUrl);
    BrowserLog("load %s", reinterpret_cast<const char*>(utf8->Ptr()));
    iWebContainer->WebView().load(reinterpret_cast<const char*>(utf8->Ptr()));
    CleanupStack::PopAndDestroy(utf8);
}

void CBrowserAppUi::NetworkConnectionReady(TInt)
{
    if (iClosing || !iPendingUrl)
        return;
    HBufC* url = iPendingUrl;
    iPendingUrl = nullptr;
    CleanupStack::PushL(url);
    LoadNowL(*url);
    CleanupStack::PopAndDestroy(url);
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
    if (iClosing)
        return;
    WebView& webView = iWebContainer->WebView();
    switch (aCommand) {
    case ECmdOptions:
        ShowOptionsMenuL();
        break;
    case ECmdGoTo:
        FocusUrlBarL();
        break;
    case ECmdBookmarks:
        ShowPageListL(_L("Bookmarks"), *iBookmarks, KNullDesC);
        break;
    case ECmdAddBookmark:
        if (iUrl)
            iBookmarks->AddL(*iUrl, iTitle ? static_cast<const TDesC&>(*iTitle) : KNullDesC());
        break;
    case ECmdRemoveBookmark:
        if (iUrl && iBookmarks->Find(*iUrl) != KErrNotFound)
            iBookmarks->Remove(iBookmarks->Find(*iUrl));
        break;
    case ECmdHistory:
        if (ShowPageListL(_L("History"), *iHistory, _L("Clear history")))
            iHistory->Reset();
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
        BrowserLog("exit diagnostic: exit command %d", aCommand);
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
    CDesCArrayFlat* items = new (ELeave) CDesCArrayFlat(8);
    CleanupStack::PushL(items);
    items->AppendL(_L("Go to address"));
    commands.AppendL(ECmdGoTo);
    if (iBookmarks->Count()) {
        items->AppendL(_L("Bookmarks"));
        commands.AppendL(ECmdBookmarks);
    }
    if (iUrl && iBookmarks->Find(*iUrl) != KErrNotFound) {
        items->AppendL(_L("Remove bookmark"));
        commands.AppendL(ECmdRemoveBookmark);
    } else if (iUrl) {
        items->AppendL(_L("Add bookmark"));
        commands.AppendL(ECmdAddBookmark);
    }
    if (iHistory->Count()) {
        items->AppendL(_L("History"));
        commands.AppendL(ECmdHistory);
    }
    if (webView.canGoForward()) {
        items->AppendL(_L("Forward"));
        commands.AppendL(ECmdForward);
    }
    items->AppendL(iLoading ? _L("Stop") : _L("Reload"));
    commands.AppendL(iLoading ? ECmdStop : ECmdReload);
    items->AppendL(_L("Exit"));
    commands.AppendL(ECmdExit);
    TInt index = ShowPopupListL(KNullDesC, *items, 0);
    SetSoftkeysL();
    TInt command = index >= 0 ? commands[index] : 0;
    CleanupStack::PopAndDestroy(2, &commands);
    if (command)
        HandleCommandL(command);
}

TBool CBrowserAppUi::ShowPageListL(const TDesC& aTitle, const CPageList& aPages, const TDesC& aExtraItem)
{
    CDesCArrayFlat* items = new (ELeave) CDesCArrayFlat(Max(aPages.Count() + 1, 1));
    CleanupStack::PushL(items);
    for (TInt i = 0; i < aPages.Count(); ++i)
        items->AppendL(aPages.Label(i));
    if (aExtraItem.Length())
        items->AppendL(aExtraItem);
    TInt index = ShowPopupListL(aTitle, *items, 0);
    SetSoftkeysL();
    CleanupStack::PopAndDestroy(items);
    if (index < 0)
        return EFalse;
    if (index >= aPages.Count())
        return ETrue;
    // The list may change once the page loads.
    HBufC* url = aPages.Url(index).AllocLC();
    LoadUrlL(*url);
    CleanupStack::PopAndDestroy(url);
    return EFalse;
}

void CBrowserAppUi::WebTitleChanged(const TDesC& aTitle)
{
    if (iClosing)
        return;
    HBufC* title = aTitle.AllocL();
    User::Free(iTitle);
    iTitle = title;
    if (iUrl)
        iHistory->SetTitleL(*iUrl, aTitle);
}

void CBrowserAppUi::WebUrlChanged(const TDesC& aUrl)
{
    if (iClosing)
        return;
    iUrlBar->SetUrlL(aUrl);
    if (iUrl && *iUrl == aUrl)
        return;
    HBufC* url = aUrl.AllocL();
    User::Free(iUrl);
    iUrl = url;
    // The title comes after the address; until then the page has none.
    User::Free(iTitle);
    iTitle = nullptr;
    _LIT(KAboutBlank, "about:blank");
    if (aUrl.Length() && aUrl != KAboutBlank)
        iHistory->AddL(aUrl, KNullDesC);
}

void CBrowserAppUi::WebProgressChanged(TBool aLoading, TInt aPercent)
{
    if (iClosing)
        return;
    iLoading = aLoading;
    iProgress = aPercent;
    iUrlBar->SetProgress(aLoading, aPercent);
}

TInt CBrowserAppUi::WebSelectPopupL(const Text* aItems, TInt aCount, TInt aSelected)
{
    if (iClosing || iShowingPopup || aCount <= 0)
        return -1;
    CDesCArrayFlat* items = new (ELeave) CDesCArrayFlat(aCount);
    CleanupStack::PushL(items);
    for (TInt i = 0; i < aCount; ++i)
        items->AppendL(TPtrC(reinterpret_cast<const TText*>(aItems[i].characters), aItems[i].length));
    iShowingPopup = ETrue;
    TInt index = -1;
    TRAPD(error, index = ShowPopupListL(KNullDesC, *items, aSelected));
    iShowingPopup = EFalse;
    SetSoftkeysL();
    CleanupStack::PopAndDestroy(items);
    User::LeaveIfError(error);
    return index;
}

void CBrowserAppUi::WebPointerDownL()
{
    // Touching the page leaves the address bar and its suggestions.
    if (iUrlBar->IsFocused())
        FocusPageL();
}

class CBrowserDocument : public CAknDocument {
public:
    SYMBIAN_BROWSER_NATIVE_DELETE
    CBrowserDocument(CEikApplication& aApp)
        : CAknDocument(aApp)
    {
    }

private:
    CEikAppUi* CreateAppUiL() { return new (ELeave) CBrowserAppUi; }
};

class CBrowserApplication : public CAknApplication {
public:
    SYMBIAN_BROWSER_NATIVE_DELETE

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
