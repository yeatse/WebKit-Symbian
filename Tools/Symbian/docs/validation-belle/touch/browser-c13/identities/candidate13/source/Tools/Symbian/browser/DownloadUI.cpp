#include "DownloadUI.h"

#include <DownloadFilename.h>
#include <AknCommonDialogsDynMem.h>
#include <MAknFileSelectionObserver.h>
#include <aknnotewrappers.h>
#include <f32file.h>

using namespace WebKitSymbian;

namespace {

TPtrC NativeText(Text text)
{
    return text.characters && text.length > 0
        ? TPtrC(reinterpret_cast<const TText*>(text.characters), text.length) : TPtrC();
}

void InformationL(const TDesC& text)
{
    // ExecuteLD owns the native dialog, including its native deleting dtor.
    (new (ELeave) CAknInformationNote(EFalse))->ExecuteLD(text);
}

class DestinationObserver final : public MAknFileSelectionObserver {
public:
    explicit DestinationObserver(RFs& fs) : m_fs(fs) { }

    TBool OkToExitL(const TDesC& directory, const TEntry& entry) final
    {
        TParse path;
        User::LeaveIfError(path.Set(entry.iName, &directory, nullptr));
        const TDesC& fullName = path.FullName();
        if (!isValidDownloadDestination(reinterpret_cast<const uint16_t*>(fullName.Ptr()), fullName.Length())) {
            InformationL(_L("Choose a valid local filename."));
            return EFalse;
        }
        TEntry existing;
        TInt error = m_fs.Entry(fullName, existing);
        if (!error) {
            InformationL(_L("File already exists. Choose another name."));
            return EFalse;
        }
        User::LeaveIfError(error == KErrNotFound ? KErrNone : error);
        return ETrue;
    }

private:
    RFs& m_fs;
};

} // namespace

bool ChooseNativeDownloadDestination(Text suggestedFilename, TextResult& result, int& nativeError)
{
    TBool accepted = EFalse;
    TRAPD(error, {
        TFileName path;
        const TPtrC suggested = NativeText(suggestedFilename);
        if (!suggested.Length() || suggested.Length() > path.MaxLength())
            User::Leave(KErrBadName);
        path.Copy(suggested);
        RFs fs;
        User::LeaveIfError(fs.Connect());
        CleanupClosePushL(fs);
        DestinationObserver observer(fs);
        const TInt memories = AknCommonDialogsDynMem::EMemoryTypePhone
            | AknCommonDialogsDynMem::EMemoryTypeInternalMassStorage
            | AknCommonDialogsDynMem::EMemoryTypeMMCExternal;
        // The two resource arguments select the folder-browsing overload
        // (ordinal 123); zero requests the firmware's default resources.
        accepted = AknCommonDialogsDynMem::RunSaveDlgLD(memories, path, 0, 0, &observer);
        if (accepted) {
            // Recheck the final path even if a firmware bypasses its observer.
            // The file adapter repeats this and closes the later rename race.
            if (!isValidDownloadDestination(reinterpret_cast<const uint16_t*>(path.Ptr()), path.Length()))
                User::Leave(KErrBadName);
            TEntry existing;
            TInt lookup = fs.Entry(path, existing);
            User::LeaveIfError(lookup == KErrNotFound ? KErrNone : (!lookup ? KErrAlreadyExists : lookup));
            result.setText({ reinterpret_cast<const uint16_t*>(path.Ptr()), path.Length() });
        }
        CleanupStack::PopAndDestroy(&fs);
    });
    nativeError = error;
    return !error && accepted;
}

void ShowNativeDownloadResult(Text path, DownloadFailure failure, int nativeError, int cleanupError, Text description)
{
    TBuf<512> message;
    if (failure == DownloadFailure::None) {
        message.Copy(_L("Saved: "));
        message.Append(NativeText(path).Left(256));
    } else if (failure == DownloadFailure::Cancelled)
        message.Copy(_L("Download cancelled."));
    else {
        message.Copy(_L("Download failed: "));
        message.Append(NativeText(description).Left(300));
        if (nativeError) {
            message.Append(_L(" ("));
            message.AppendNum(nativeError);
            message.Append(_L(")"));
        }
    }
    if (cleanupError) {
        message.Append(_L(" Temporary file cleanup failed: "));
        message.AppendNum(cleanupError);
    }
    TRAP_IGNORE(InformationL(message));
}
