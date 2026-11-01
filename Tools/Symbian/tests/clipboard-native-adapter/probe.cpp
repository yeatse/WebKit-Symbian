// Exercises the actual production ClipboardNative.cpp. The exclusive emulator
// operator must preserve and restore the original native clipboard store.
#include "ClipboardNative.h"
#include <baclipb.h>
#include <e32base.h>
#include <f32file.h>
#include <s32file.h>
#include <s32std.h>
#include <txtclipboard.h>
#include <txtetext.h>
#include <cstdio>

using namespace WebKitSymbian;
static FILE* output;
static unsigned passed, failed, nativeErrors;
// Do not log format arguments: they must never become a clipboard disclosure.
void BrowserLog(const char*, ...) { ++nativeErrors; }
static void check(bool value, const char* name)
{
    std::fprintf(output, "%s: %s\n", value ? "PASS" : "FAIL", name);
    std::fflush(output);
    value ? ++passed : ++failed;
}
class Result final : public TextResult {
public:
    void setText(Text value) final
    {
        ++calls;
        if (value.length < 0 || value.length > text.MaxLength() || (value.length && !value.characters)) {
            valid = false;
            return;
        }
        text.Copy(TPtrC(value.characters, value.length));
    }
    unsigned calls { 0 };
    bool valid { true };
    TBuf<128> text;
};
static const TUint16 source[] = { 'A', 0x4f60, 0x597d, 0xd83d, 0xde42, '\r', '\n', 'B', '\r', 'C', '\n', 'D', 0x2028, 'E', 0x2029 };
static const TUint16 expected[] = { 'A', 0x4f60, 0x597d, 0xd83d, 0xde42, '\n', 'B', '\n', 'C', '\n', 'D', '\n', 'E', '\n' };
static Text original() { return { source, sizeof(source) / sizeof(source[0]) }; }
static bool readExpected()
{
    Result result;
    return NativeClipboard().readPlainText(result) && result.calls == 1 && result.valid
        && result.text == TPtrC(expected, sizeof(expected) / sizeof(expected[0]));
}
static TFileName clipboardPath()
{
    TFileName path(_L("\\System\\Data\\Clpboard.cbd"));
    path.Insert(0, CClipboard::ClipboardFileDrive());
    return path;
}
static void malformedTextL(RFs& fs, TInt length, const TUint8* bytes, TInt count)
{
    CClipboard* clipboard = CClipboard::NewForWritingLC(fs);
    RStoreWriteStream stream;
    TStreamId id = stream.CreateLC(clipboard->Store());
    stream.WriteInt32L(length);
    if (count)
        stream.WriteL(bytes, count);
    stream.CommitL();
    CleanupStack::PopAndDestroy(&stream);
    clipboard->StreamDictionary().AssignL(KClipboardUidTypePlainText, id);
    clipboard->CommitL();
    CleanupStack::PopAndDestroy(clipboard);
}
static void oversizedRootL(RFs& fs)
{
    CDirectFileStore* store = CDirectFileStore::ReplaceLC(fs, clipboardPath(), EFileWrite);
    const TUid unicodeUID = { 0x10003a10 };
    store->SetTypeL(TUidType(KDirectFileStoreLayoutUid, unicodeUID));
    RStoreWriteStream stream;
    TStreamId id = stream.CreateLC(*store);
    stream << TCardinality(65);
    stream.CommitL();
    CleanupStack::PopAndDestroy(&stream);
    store->SetRootL(id);
    store->CommitL();
    CleanupStack::PopAndDestroy(store);
}
static bool rejectedRead()
{
    Result result;
    return !NativeClipboard().readPlainText(result) && !result.calls;
}
static bool allocationsStable(bool expectFailure)
{
    // Warm DLL-local lazily allocated state before measuring repeated reads.
    for (unsigned i = 0; i < 4; ++i) {
        if (expectFailure ? !rejectedRead() : !readExpected())
            return false;
    }
    TInt beforeBytes = 0, afterBytes = 0;
    TInt beforeCells = User::AllocSize(beforeBytes);
    TInt beforeProcess = 0, beforeThread = 0, afterProcess = 0, afterThread = 0;
    RThread().HandleCount(beforeProcess, beforeThread);
    for (unsigned i = 0; i < 64; ++i) {
        if (expectFailure ? !rejectedRead() : !readExpected())
            return false;
    }
    TInt afterCells = User::AllocSize(afterBytes);
    RThread().HandleCount(afterProcess, afterThread);
    return beforeCells == afterCells && beforeBytes == afterBytes
        && beforeProcess == afterProcess && beforeThread == afterThread;
}
static void runL()
{
    RFs fs;
    User::LeaveIfError(fs.Connect());
    CleanupClosePushL(fs);
    auto& clipboard = NativeClipboard();
    check(clipboard.writePlainText(original()), "production write Chinese emoji and mixed line endings");
    check(clipboard.hasPlainText(), "native plain stream is present");
    check(readExpected(), "production read returns exact normalized UTF16 once");
    {
        CClipboard* native = CClipboard::NewForReadingLC(fs);
        CPlainText* text = CPlainText::NewL();
        CleanupStack::PushL(text);
        TInt inserted = text->PasteFromStoreL(native->Store(), native->StreamDictionary(), 0);
        TBuf<128> actual;
        text->Extract(actual);
        check(inserted == 14 && actual.Length() == 14 && actual[5] == 0x2029 && actual[11] == 0x2028,
            "independent CPlainText reads real native text stream");
        CleanupStack::PopAndDestroy(text);
        CleanupStack::PopAndDestroy(native);
    }
    check(clipboard.writePlainText({ nullptr, 0 }), "write explicit empty string");
    check(clipboard.hasPlainText(), "empty text retains plain stream presence");
    Result empty;
    check(clipboard.readPlainText(empty) && empty.calls == 1 && empty.valid && !empty.text.Length(), "empty text invokes callback exactly once");
    check(clipboard.writePlainText(original()), "restore owned sentinel text");
    check(!clipboard.writePlainText({ source, maximumClipboardTextLength + 1 }), "oversized write rejected before reading source");
    check(readExpected(), "oversized write preserves previous contents");
    check(!clipboard.writePlainText({ source, -1 }), "negative write length rejected");
    check(!clipboard.writePlainText({ nullptr, 1 }), "null nonempty write rejected");
    check(readExpected(), "invalid pointer and length preserve previous contents");
    User::LeaveIfError(fs.SetAtt(clipboardPath(), KEntryAttReadOnly, 0));
    check(!clipboard.writePlainText(original()), "readonly native store refuses write");
    check(readExpected(), "readonly store remains readable and unchanged");
    User::LeaveIfError(fs.SetAtt(clipboardPath(), 0, KEntryAttReadOnly));
    {
        RFile lock;
        User::LeaveIfError(lock.Open(fs, clipboardPath(), EFileRead | EFileShareExclusive));
        CleanupClosePushL(lock);
        check(rejectedRead(), "exclusive native store lock rejects read without callback");
        check(!clipboard.writePlainText(original()), "exclusive native store lock refuses write");
        CleanupStack::PopAndDestroy(&lock);
    }
    check(readExpected(), "locked write failure preserves previous contents");
    check(clipboard.clear(), "production clear removes owned store");
    check(!clipboard.hasPlainText() && rejectedRead(), "missing store has no text and no callback");
    oversizedRootL(fs);
    check(!clipboard.hasPlainText() && rejectedRead(), "dictionary count above64 rejected before native parsing");
    const TUint8 zero = 0;
    malformedTextL(fs, -1, &zero, 1);
    check(!clipboard.hasPlainText() && rejectedRead(), "negative serialized text length rejected");
    malformedTextL(fs, maximumClipboardTextLength + 1, &zero, 1);
    check(!clipboard.hasPlainText() && rejectedRead(), "oversized serialized text length rejected");
    const TUint8 reserved = 0x0c;
    malformedTextL(fs, 1, &reserved, 1);
    check(rejectedRead(), "reserved SCSU command rejected without callback");
    const TUint8 truncated = 0x0f;
    malformedTextL(fs, 1, &truncated, 1);
    check(rejectedRead(), "truncated SCSU command rejected without callback");
    const TUint8 invalidFieldSet = 2;
    malformedTextL(fs, 0, &invalidFieldSet, 1);
    check(rejectedRead(), "invalid field-set flag rejected");
    check(allocationsStable(true), "repeated corrupt reads release native heap cells and handles");
    check(clipboard.writePlainText(original()), "valid write recovers after malformed store");
    check(allocationsStable(false), "repeated valid reads release native heap cells and handles");
    {
        RFile file;
        User::LeaveIfError(file.Open(fs, clipboardPath(), EFileWrite | EFileShareExclusive));
        CleanupClosePushL(file);
        User::LeaveIfError(file.SetSize(4 * 1024 * 1024 + 1));
        CleanupStack::PopAndDestroy(&file);
    }
    check(!clipboard.hasPlainText() && rejectedRead(), "oversized native store rejected before allocation");
    check(clipboard.clear() && !clipboard.hasPlainText(), "final clear leaves no owned test data");
    CleanupStack::PopAndDestroy(&fs);
}
int main()
{
    output = std::fopen("C:/Data/WebKit/clipboard-adapter-probe.log", "wb");
    if (!output)
        return 2;
    std::fprintf(output, "BEGIN clipboard-adapter-probe version=1\n");
    CTrapCleanup* cleanup = CTrapCleanup::New();
    if (!cleanup)
        check(false, "native cleanup stack allocated");
    else {
        TRAPD(error, runL());
        if (error) {
            std::fprintf(output, "ERROR probe Leave=%d\n", error);
            check(false, "native probe setup and execution");
        }
    }
    delete cleanup;
    std::fprintf(output, "RESULT {\"probe\":\"clipboard-adapter-probe\",\"passed\":%u,\"failed\":%u,\"nativeErrors\":%u}\n", passed, failed, nativeErrors);
    std::fprintf(output, "%s: clipboard-adapter-probe %u/%u\nclipboard-adapter-probe exiting %d\n", failed ? "FAIL" : "PASS", passed, passed + failed, failed ? 1 : 0);
    std::fclose(output);
    return failed ? 1 : 0;
}
