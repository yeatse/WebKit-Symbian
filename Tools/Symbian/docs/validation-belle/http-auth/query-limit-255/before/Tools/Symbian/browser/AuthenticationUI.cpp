#include "AuthenticationUI.h"

#include "browser_ui.rsg"
#include <AknQueryDialog.h>
#include <avkon.hrh>

using namespace WebKitSymbian;

namespace {

constexpr TInt maximumCredentialLength = 256;
constexpr TInt maximumOriginLength = 512;

} // namespace

class CAuthenticationQueryDialog final : public CAknTextQueryDialog {
public:
    SYMBIAN_BROWSER_NATIVE_DELETE
    CAuthenticationQueryDialog(TDes& value, CAuthenticationQueryDialog*& active)
        : CAknTextQueryDialog(value)
        , iActive(active)
    {
        iActive = this;
        SetMaxLength(maximumCredentialLength);
        SetPredictiveTextInputPermitted(EFalse);
    }

    ~CAuthenticationQueryDialog() { iActive = nullptr; }
    void CancelL() { TryExitL(EAknSoftkeyCancel); }

private:
    // HTTP permits empty usernames/passwords. The official AVKON implementation
    // uses this visibility in OkToExitL(), then copies even an empty descriptor.
    void UpdateLeftSoftKeyL() override { MakeLeftSoftkeyVisible(ETrue); }
    CAuthenticationQueryDialog*& iActive;
};

namespace {

Text ToText(const TDesC& value)
{
    return { reinterpret_cast<const uint16_t*>(value.Ptr()), value.Length() };
}

} // namespace

void CAuthenticationUI::Release()
{
    if (!--iReferences)
        delete this;
}

void CAuthenticationUI::Cancel(uint64_t identifier)
{
    if (!iBusy || (identifier && identifier != iIdentifier))
        return;
    iCancelled = true;
    if (iDialog)
        TRAP_IGNORE(iDialog->CancelL());
}

bool CAuthenticationUI::Run(uint64_t identifier, Text origin, unsigned attempt, TextResult& username, TextResult& password)
{
    if (iBusy || !identifier || !origin.characters || origin.length <= 0
        || origin.length > maximumOriginLength || !attempt || attempt > 3)
        return false;
    ++iReferences;
    iBusy = true;
    iCancelled = false;
    iIdentifier = identifier;
    TBuf<maximumCredentialLength> user;
    TBuf<maximumCredentialLength> secret;
    TBool accepted = EFalse;
    TRAPD(error, accepted = RunL(origin, attempt, user, secret));
    bool success = !error && accepted && !iCancelled;
    if (success) {
        username.setText(ToText(user));
        password.setText(ToText(secret));
    }
    // Clear the transfer buffer on success, cancellation and Leave. Never log
    // either credential or put it in the page, URL, clipboard or preferences.
    secret.SetLength(secret.MaxLength());
    volatile TText* characters = const_cast<TText*>(secret.Ptr());
    for (TInt i = 0; i < secret.Length(); ++i)
        characters[i] = 0;
    iIdentifier = 0;
    iBusy = false;
    Release();
    return success;
}

TBool CAuthenticationUI::RunL(Text origin, unsigned attempt, TDes& username, TDes& password)
{
    TBuf<640> heading;
    heading.Append(_L("HTTP sign in ("));
    heading.AppendNum(attempt);
    heading.Append(_L("/3)\n"));
    heading.Append(TPtrC(reinterpret_cast<const TText*>(origin.characters), origin.length));
    const TInt originHeadingLength = heading.Length();
    heading.Append(_L("\nUsername"));
    auto* userDialog = new (ELeave) CAuthenticationQueryDialog(username, iDialog);
    if (!userDialog->ExecuteLD(R_BROWSER_PROMPT, heading) || iCancelled)
        return EFalse;
    heading.SetLength(originHeadingLength);
    heading.Append(_L("\nPassword"));
    // ECodeLayout uses the native masked secret editor, not an ordinary EDWIN.
    auto* passwordDialog = new (ELeave) CAuthenticationQueryDialog(password, iDialog);
    return passwordDialog->ExecuteLD(R_BROWSER_AUTH_PASSWORD, heading) && !iCancelled;
}
