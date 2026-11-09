// A small AVKON application that exercises what split-e32.py rewrites:
// calls, function pointers, vtables, exceptions and leaves across DLLs.
// It shows the results, writes them to C:\Data\splitdemo\<name>.txt, and
// exits by itself after 20 seconds (or at any key).
#include "demo.h"

#include <apaapp.h>
#include <aknapp.h>
#include <aknappui.h>
#include <akndoc.h>
#include <coecntrl.h>
#include <e32base.h>
#include <eikenv.h>
#include <f32file.h>

// eikstart.h, which the SDK lacks.
namespace EikStart {
IMPORT_C TInt RunApplication(TApaApplicationFactory aApplicationFactory);
}

#define DEMO_NATIVE_DELETE \
    static void operator delete(TAny* aPointer) noexcept { User::Free(aPointer); } \
    static void operator delete(TAny* aPointer, TLeave) noexcept { User::Free(aPointer); }

namespace {

const TUid KUidDemo = { static_cast<TInt32>(DEMO_UID3) };
#define DEMO_LIT(name, text) _LIT(name, text)
DEMO_LIT(KDemoName, DEMO_NAME_TEXT);
const TInt KMaxLines = 16;
_LIT(KOk, "ok");
_LIT(KBad, "BAD");
_LIT(KNone, "");

class TDemoLog {
public:
    TDemoLog() : iCount(0) { }

    void Add(const TDesC& aLine)
    {
        if (iCount < KMaxLines)
            iLines[iCount++] = aLine.Left(iLines[0].MaxLength());
        Write(aLine);
    }

    // Each line reaches the file before the next test runs, so a panic
    // leaves the last step that worked.
    static void Write(const TDesC& aLine)
    {
        RFs fs;
        if (fs.Connect() != KErrNone)
            return;
        _LIT(KDir, "C:\\Data\\splitdemo\\");
        fs.MkDirAll(KDir);
        TFileName path(KDir);
        path.Append(KDemoName);
        path.Append(_L(".txt"));
        RFile file;
        TInt error = file.Open(fs, path, EFileWrite | EFileShareAny);
        if (error == KErrNotFound)
            error = file.Create(fs, path, EFileWrite | EFileShareAny);
        if (error == KErrNone) {
            TInt end = 0;
            file.Seek(ESeekEnd, end);
            TBuf8<128> line;
            line.Copy(aLine.Left(126));
            line.Append(_L8("\r\n"));
            file.Write(line);
            file.Close();
        }
        fs.Close();
    }

    TInt iCount;
    TBuf<64> iLines[KMaxLines];
};

class CDemoView : public CCoeControl {
public:
    DEMO_NATIVE_DELETE

    explicit CDemoView(const TDemoLog& aLog) : iLog(aLog) { }

    void ConstructL(const TRect& aRect)
    {
        CreateWindowL();
        SetRect(aRect);
        ActivateL();
    }

private:
    void Draw(const TRect&) const override
    {
        CWindowGc& gc = SystemGc();
        gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
        gc.SetBrushColor(KRgbWhite);
        gc.Clear(Rect());
        const CFont* font = CEikonEnv::Static()->NormalFont();
        gc.UseFont(font);
        gc.SetPenColor(KRgbBlack);
        TInt y = font->AscentInPixels() + 8;
        for (TInt i = 0; i < iLog.iCount; ++i) {
            gc.DrawText(iLog.iLines[i], TPoint(8, y));
            y += font->HeightInPixels() + 6;
        }
        gc.DiscardFont();
    }

    const TDemoLog& iLog;
};

class CDemoAppUi : public CAknAppUi {
public:
    DEMO_NATIVE_DELETE

    ~CDemoAppUi()
    {
        delete iTimer;
        if (iView) {
            RemoveFromStack(iView);
            delete iView;
        }
    }

    void ConstructL() override
    {
        BaseConstructL(TInt(EAknEnableSkin) | TInt(ENoAppResourceFile) | TInt(EAknTouchCompatible) | TInt(EAknSingleClickCompatible));
        RunTests();
        iView = new (ELeave) CDemoView(iLog);
        iView->ConstructL(ClientRect());
        AddToStackL(iView);
        iTimer = CPeriodic::NewL(CActive::EPriorityStandard);
        iTimer->Start(20000000, 20000000, TCallBack(Timeout, this));
    }

private:
    void Result(const TDesC& aName, TInt aValue, TInt aExpected, TBool aCheck)
    {
        TBuf<64> line;
        line.Format(_L("%S %d %S"), &aName, aValue,
            aCheck ? (aValue == aExpected ? &KOk() : &KBad()) : &KNone());
        iLog.Add(line);
    }

    void RunTests()
    {
        TBuf<64> line;
        line.Format(_L("%S UID %08x"), &KDemoName(), KUidDemo.iUid);
        iLog.Add(line);
        Result(_L("init"), demo_bulk_init_value(), 0, EFalse);
        Result(_L("chain"), demo_bulk_chain(17, 40), 0, EFalse);
        Result(_L("cchain"), demo_bulk_const_chain(250, 40), 0, EFalse);
        // Square 7*7*10+4 + triangle 24*10+3, plus one.
        Result(_L("virtual"), demo_last_virtual(7), 490 + 4 + 240 + 3 + 1, ETrue);
        Result(_L("throw"), demo_first_catch(7), 7, ETrue);
        Result(_L("leave"), demo_first_trap(KErrNotSupported), KErrNotSupported, ETrue);
        Result(_L("libc"), demo_libc_length(demo_last_string()), 35, ETrue);
        iLog.Add(_L("done; exits in 20 s or at any key"));
    }

    TKeyResponse HandleKeyEventL(const TKeyEvent&, TEventCode aType) override
    {
        if (aType == EEventKey)
            Exit();
        return EKeyWasConsumed;
    }

    void HandleCommandL(TInt aCommand) override
    {
        if (aCommand == EEikCmdExit || aCommand == EAknSoftkeyExit || aCommand == EAknSoftkeyBack)
            Exit();
    }

    static TInt Timeout(TAny* aSelf)
    {
        static_cast<CDemoAppUi*>(aSelf)->Exit();
        return 0;
    }

    TDemoLog iLog;
    CDemoView* iView = nullptr;
    CPeriodic* iTimer = nullptr;
};

class CDemoDocument : public CAknDocument {
public:
    DEMO_NATIVE_DELETE
    explicit CDemoDocument(CEikApplication& aApp) : CAknDocument(aApp) { }

private:
    CEikAppUi* CreateAppUiL() override { return new (ELeave) CDemoAppUi; }
};

class CDemoApplication : public CAknApplication {
public:
    DEMO_NATIVE_DELETE

private:
    TUid AppDllUid() const override { return KUidDemo; }
    CApaDocument* CreateDocumentL() override { return new (ELeave) CDemoDocument(*this); }
    TFileName ResourceFileName() const override { return KNullDesC(); }
};

CApaApplication* NewApplication()
{
    return new CDemoApplication;
}

}

int main()
{
    TDemoLog::Write(_L("main"));
    return EikStart::RunApplication(NewApplication);
}
