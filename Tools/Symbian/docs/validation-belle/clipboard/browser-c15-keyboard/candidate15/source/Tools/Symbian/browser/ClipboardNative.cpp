#include "ClipboardNative.h"
#include "BrowserLog.h"

#include <baclipb.h>
#include <e32base.h>
#include <f32file.h>
#include <hal.h>
#include <s32file.h>
#include <s32std.h>
#include <s32ucmp.h>
#include <txtclipboard.h>
#include <txtetext.h>

using namespace WebKitSymbian;

namespace {

constexpr TInt KMaximumStoreBytes = 4 * 1024 * 1024;
constexpr TInt KMaximumFormats = 64;
const TUid KUnicodeClipboardFileUid = { 0x10003A10 };

void CheckDriveL()
{
    // CClipboard panics for an invalid HAL drive instead of leaving.
    TInt drive = -1;
    User::LeaveIfError(HAL::Get(HAL::EClipboardDrive, drive));
    if (drive < EDriveA || drive > EDriveY)
        User::Leave(KErrNotSupported);
}

CFileStore* PreflightStoreLC(RFs& fs)
{
    CheckDriveL();
    TFileName path(_L("\\System\\Data\\Clpboard.cbd"));
    path.Insert(0, CClipboard::ClipboardFileDrive());
    // Keep this readers-only handle alive through NewForReadingLC. Writers
    // cannot replace the validated file between the check and dictionary read.
    CFileStore* store = CDirectFileStore::OpenLC(fs, path, EFileRead | EFileShareReadersOnly);
    if (store->Type() != TUidType(KDirectFileStoreLayoutUid, KUnicodeClipboardFileUid))
        User::Leave(KErrCorrupt);
    TInt size = 0;
    User::LeaveIfError(store->File().Size(size));
    if (size < 0 || size > KMaximumStoreBytes)
        User::Leave(KErrTooBig);
    RStoreReadStream root;
    root.OpenLC(*store, store->Root());
    TCardinality formats;
    root >> formats;
    if (static_cast<TInt>(formats) < 0 || static_cast<TInt>(formats) > KMaximumFormats)
        User::Leave(KErrTooBig);
    CleanupStack::PopAndDestroy(&root);
    return store;
}

// Native editors use paragraph separators. Plain web text uses LF. Preserve
// the UTF-16 code units, including surrogate pairs, apart from line endings.
void NormalizeForWeb(TDes& text)
{
    TInt output = 0;
    for (TInt input = 0; input < text.Length(); ++input) {
        TUint16 character = text[input];
        if (character == '\r') {
            if (input + 1 < text.Length() && text[input + 1] == '\n')
                ++input;
            character = '\n';
        } else if (character == 0x2028 || character == 0x2029)
            character = '\n';
        text[output++] = character;
    }
    text.SetLength(output);
}

bool ReadL(TextResult* result)
{
    RFs fs;
    User::LeaveIfError(fs.Connect());
    CleanupClosePushL(fs);
    CFileStore* preflight = PreflightStoreLC(fs);
    CClipboard* clipboard = CClipboard::NewForReadingLC(fs);
    TStreamId id = clipboard->StreamDictionary().At(KClipboardUidTypePlainText);
    if (id == KNullStreamId) {
        CleanupStack::PopAndDestroy(3, &fs);
        return false;
    }
    RStoreReadStream stream;
    stream.OpenLC(clipboard->Store(), id);
    TInt length = stream.ReadInt32L();
    if (length < 0 || length > maximumClipboardTextLength)
        User::Leave(KErrTooBig);
    if (result) {
        HBufC* text = HBufC::NewLC(length);
        TPtr buffer = text->Des();
        buffer.SetLength(length);
        TMemoryUnicodeSink sink(const_cast<TUint16*>(buffer.Ptr()));
        TUnicodeExpander expander;
        TInt outputWords = 0;
        expander.ExpandL(sink, stream, length, KMaximumStoreBytes, &outputWords);
        if (outputWords != length)
            User::Leave(KErrCorrupt);
        // CPlainText's clipboard stream ends its text with a field-set flag.
        // Do not instantiate native field graphs from clipboard data. Only
        // the text representation is supported by this bridge.
        if (stream.ReadUint8L() > 1)
            User::Leave(KErrCorrupt);
        NormalizeForWeb(buffer);
        result->setText({ buffer.Ptr(), buffer.Length() });
        CleanupStack::PopAndDestroy(text);
    }
    CleanupStack::PopAndDestroy(&stream);
    CleanupStack::PopAndDestroy(clipboard);
    CleanupStack::PopAndDestroy(preflight);
    CleanupStack::PopAndDestroy(&fs);
    return true;
}

void WriteL(Text value)
{
    CheckDriveL();
    if (value.length < 0 || value.length > maximumClipboardTextLength || (value.length && !value.characters))
        User::Leave(KErrArgument);
    // Normalize before opening the writable clipboard so rejected/allocation
    // failures here leave its existing contents untouched.
    HBufC* normalized = HBufC::NewLC(value.length);
    TPtr buffer = normalized->Des();
    for (TInt i = 0; i < value.length; ++i) {
        TUint16 character = value.characters[i];
        if (character == '\r') {
            if (i + 1 < value.length && value.characters[i + 1] == '\n')
                ++i;
            character = 0x2029;
        } else if (character == '\n')
            character = 0x2029;
        buffer.Append(character);
    }
    CPlainText* text = CPlainText::NewL();
    CleanupStack::PushL(text);
    if (buffer.Length())
        text->InsertL(0, buffer);
    RFs fs;
    User::LeaveIfError(fs.Connect());
    CleanupClosePushL(fs);
    CClipboard* clipboard = CClipboard::NewForWritingLC(fs);
    if (text->DocumentLength())
        text->CopyToStoreL(clipboard->Store(), clipboard->StreamDictionary(), 0, text->DocumentLength());
    else {
        // CPlainText::CopyToStoreL deliberately omits a zero-length selection.
        // An empty web string is nevertheless a present plain-text value.
        // Emit the same canonical stream: signed length, SCSU text, fields flag.
        RStoreWriteStream stream;
        TStreamId id = stream.CreateLC(clipboard->Store());
        stream.WriteInt32L(0);
        stream.WriteUint8L(0);
        stream.CommitL();
        clipboard->StreamDictionary().AssignL(KClipboardUidTypePlainText, id);
        CleanupStack::PopAndDestroy(&stream);
    }
    clipboard->CommitL();
    CleanupStack::PopAndDestroy(clipboard);
    CleanupStack::PopAndDestroy(&fs);
    CleanupStack::PopAndDestroy(text);
    CleanupStack::PopAndDestroy(normalized);
}

class SystemClipboard final : public ClipboardClient {
public:
    bool hasPlainText() final
    {
        bool present = false;
        TRAPD(error, present = ReadL(nullptr));
        return error == KErrNone && present;
    }

    bool readPlainText(TextResult& result) final
    {
        bool present = false;
        TRAPD(error, present = ReadL(&result));
        if (error && error != KErrNotFound && error != KErrPathNotFound)
            BrowserLog("clipboard read error %d", error);
        return error == KErrNone && present;
    }

    bool writePlainText(Text text) final
    {
        TRAPD(error, WriteL(text));
        if (error)
            BrowserLog("clipboard write error %d", error);
        return error == KErrNone;
    }

    bool clear() final
    {
        TRAPD(error, ClearL());
        if (error)
            BrowserLog("clipboard clear error %d", error);
        return error == KErrNone;
    }

private:
    static void ClearL()
    {
        CheckDriveL();
        RFs fs;
        User::LeaveIfError(fs.Connect());
        CleanupClosePushL(fs);
        TInt error = CClipboard::Clear(fs);
        if (error != KErrNotFound && error != KErrPathNotFound)
            User::LeaveIfError(error);
        CleanupStack::PopAndDestroy(&fs);
    }
};

} // namespace

ClipboardClient& NativeClipboard()
{
    static SystemClipboard clipboard;
    return clipboard;
}
