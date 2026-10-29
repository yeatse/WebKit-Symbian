// Native system clipboard only. The host operator must preserve the original
// clipboard store before selecting a write mode and restore it after the run.
#include <baclipb.h>
#include <e32base.h>
#include <f32file.h>
#include <txtetext.h>
#include <cstdio>

static FILE* output;
static unsigned passed;
static unsigned failed;
static void check(bool value, const char* name)
{
    std::fprintf(output, "%s: %s\n", value ? "PASS" : "FAIL", name);
    std::fflush(output);
    value ? ++passed : ++failed;
}

static const TUint16 payload[] = {
    'N','a','t','i','v','e',' ',0x4f60,0x597d,' ',0xd83d,0xde42,'\n',
    'l','i','n','e',' ','2','\n'
};

static void runL(TInt mode)
{
    RFs fs;
    User::LeaveIfError(fs.Connect());
    CleanupClosePushL(fs);
    TDriveName drive = CClipboard::ClipboardFileDrive();
    check(drive.Length() >= 2 && drive[1] == ':', "native clipboard drive resolved");
    std::fprintf(output, "INFO native clipboard drive=%c:\n", drive.Length() ? static_cast<char>(drive[0]) : '?');
    if (!mode) {
        CClipboard* clipboard = nullptr;
        TRAPD(error, clipboard = CClipboard::NewForReadingL(fs));
        // Do not extract, hash, or print the user's previous clipboard text.
        std::fprintf(output, "INFO read-only open status=%d\n", error);
        check(error == KErrNone || error == KErrNotFound || error == KErrPathNotFound,
            "read-only inspection has defined success or missing-store result");
        delete clipboard;
    } else {
        bool writing = mode == 1 || mode == 3;
        bool empty = mode == 3 || mode == 4;
        TPtrC expected(payload, empty ? 0 : sizeof(payload) / sizeof(payload[0]));
        CClipboard* clipboard = writing ? CClipboard::NewForWritingLC(fs)
                                       : CClipboard::NewForReadingLC(fs);
        CPlainText* text = CPlainText::NewL();
        CleanupStack::PushL(text);
        if (writing) {
            if (!empty)
                text->InsertL(0, expected);
            check(text->DocumentLength() == expected.Length(), "source UTF-16 length");
            text->CopyToStoreL(clipboard->Store(), clipboard->StreamDictionary(), 0,
                text->DocumentLength());
            clipboard->CommitL();
            check(true, "real clipboard store committed");
        } else {
            TInt inserted = text->PasteFromStoreL(clipboard->Store(), clipboard->StreamDictionary(), 0);
            check(inserted == expected.Length(), "clipboard UTF-16 paste length");
            TBuf<64> actual;
            text->Extract(actual);
            check(actual == expected && text->DocumentLength() == expected.Length(),
                "independent process UTF-16 contents match exactly");
        }
        CleanupStack::PopAndDestroy(text);
        CleanupStack::PopAndDestroy(clipboard);
    }
    CleanupStack::PopAndDestroy(&fs);
}

int main()
{
    output = std::fopen("C:/Data/WebKit/clipboard-probe.log", "wb");
    if (!output)
        return 2;
    TBuf<64> argument;
    TInt length = User::CommandLineLength();
    if (length < 0 || length > argument.MaxLength()) {
        std::fclose(output);
        return 2;
    }
    User::CommandLine(argument);
    argument.Trim();
    if (argument.Length() >= 2 && argument[0] == '"' && argument[argument.Length() - 1] == '"')
        argument.Copy(argument.Mid(1, argument.Length() - 2));
    TInt mode = argument == _L("inspect") ? 0 : argument == _L("write") ? 1
        : argument == _L("read") ? 2 : argument == _L("empty-write") ? 3
        : argument == _L("empty-read") ? 4 : -1;
    const char* name = mode == 0 ? "inspect" : mode == 1 ? "write" : mode == 2 ? "read"
        : mode == 3 ? "empty-write" : mode == 4 ? "empty-read" : "invalid";
    std::fprintf(output, "BEGIN native-clipboard version=1 mode=%s\n", name);
    CTrapCleanup* cleanup = CTrapCleanup::New();
    if (!cleanup || mode < 0)
        check(false, "valid mode and native cleanup stack");
    else {
        TRAPD(error, runL(mode));
        if (error) {
            std::fprintf(output, "ERROR native Leave=%d\n", error);
            check(false, "native clipboard operation");
        }
    }
    delete cleanup;
    std::fprintf(output, "RESULT {\"probe\":\"native-clipboard\",\"mode\":\"%s\",\"passed\":%u,\"failed\":%u,\"payloadUTF16Units\":%u}\n",
        name, passed, failed, static_cast<unsigned>(sizeof(payload) / sizeof(payload[0])));
    std::fprintf(output, "%s: native-clipboard %u/%u\nnative-clipboard exiting %d\n",
        failed ? "FAIL" : "PASS", passed, passed + failed, failed ? 1 : 0);
    std::fclose(output);
    return failed ? 1 : 0;
}
