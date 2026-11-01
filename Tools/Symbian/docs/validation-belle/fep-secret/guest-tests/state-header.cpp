#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using TInt = int;
using TBool = int;
constexpr TBool ETrue = 1;
constexpr TBool EFalse = 0;
constexpr int KErrNone = 0, KErrCancel = -3, KErrInUse = -14;
constexpr int KRequestPending = -2147483647;  // SDK e32const.h: -KMaxTInt.
constexpr int KAvkonFepCat = 0;
#define _L(x) x
void LogOut(int, const char*) {}
struct User {
    static void LeaveIfError(int e) { if (e) throw e; }
    static void Leave(int e) { throw e; }
};
struct Text {
    std::u16string own;
    std::u16string* p;
    Text(int = 0, int = 0) : p(&own) {}
    Text(std::u16string* data) : p(data) {}
    void Set(Text value) { p = value.p; }
    void SetLength(int n) { p->resize(n); }
    int Length() const { return p->size(); }
};
using TPtr = Text;
using TDesC = Text;
struct HBufC {
    std::u16string data;
    static HBufC* NewLC(int n);
    Text Des() { return Text(&data); }
    const char16_t* Ptr() { return data.data(); }
};
struct CleanupStack {
    static inline std::vector<HBufC*> stack;
    static void PopAndDestroy() { delete stack.back(); stack.pop_back(); }
};
HBufC* HBufC::NewLC(int n) { auto p = new HBufC; p->data.resize(n); CleanupStack::stack.push_back(p); return p; }
struct TCursorSelection {
    int cursor, anchor;
    TCursorSelection(int a, int b) : cursor(a), anchor(b) {}
};
struct Editor {
    std::u16string text;
    int anchor = 0, cursor = 0, starts = 0, commits = 0, sets = 0, cancels = 0;
    int maximum = 1024 * 1024;
    bool composition = false;
    int DocumentLengthForFep() { return text.size(); }
    int DocumentMaximumLengthForFep() { return maximum; }
    void GetEditorContentForFep(Text& out, int, int) { *out.p = text; }
    void SetCursorSelectionForFepL(TCursorSelection s) { cursor = s.cursor; anchor = s.anchor; ++sets; }
    template<class... A> void StartFepInlineEditL(Text value, int position, A&&...) {
        text = *value.p; cursor = anchor = position; composition = true; ++starts;
    }
    void UpdateFepInlineTextL(Text value, int position) { text = *value.p; cursor = anchor = position; }
    void CommitFepInlineEditL(int) { composition = false; ++commits; }
    void CancelFepInlineEdit() { if (composition) text.clear(); composition = false; ++cancels; }
};
using MCoeFepAwareTextEditor = Editor;
struct Capabilities {
    Editor* editor = nullptr;
    int rawSecret = 0;
    Editor* FepAwareTextEditor() { return editor; }
    // Mirrors official COEINPUT.CPP's unnormalized ESecretText mask return.
    TBool SupportsSecretText() const { return rawSecret; }
};
struct CCoeEnv { static int* Static() { static int v; return &v; } };
struct CHostDialogIme;
struct Status {
    int value = 0;
    CHostDialogIme* owner = nullptr;
    int Int() const { return value; }
    Status& operator=(int v) { value = v; return *this; }
};
using TRequestStatus = Status;
struct CHostBridgedImeFEP;
struct CHostDialogIme {
    CHostBridgedImeFEP* iHost = nullptr;
    Status iStatus;
    int opens = 0, cancels = 0, error = 0;
    int receivedMax = -1, receivedSecret = -1, receivedFunc = -1;
    Status* receivedStatus = nullptr;
    bool statusPendingAtDispatch = false, active = false;
    std::u16string draft;
    CHostDialogIme() { iStatus.owner = this; }
    TInt Open(const TDesC*, const TInt, const TBool);
    void SetActive() { active = true; }
    void Cancel() { ++cancels; active = false; }
    void RunL();
};
struct CHostBridgedImeFEP {
    Capabilities iInputCapabilities;
    bool iInFEPWork = false, iDialogPending = false;
    CHostDialogIme iImeDialog;
    void OpenDialogInputL(TBool = EFalse);
    void CommitChangedTextL();
    void CancelChangedTextL();
    void CancelDialogL();
};
int oldA0Calls = 0;
TInt EHUIOpenGlobalTextView(int, const TDesC*, const TInt, TRequestStatus*) {
    ++oldA0Calls;
    throw std::runtime_error("new FEP unexpectedly called legacy A0");
}
TInt EHUIOpenGlobalTextViewWithSecret(int function, const TDesC* initial,
                                    const TInt maximum, TBool secret,
                                    TRequestStatus* status) {
    auto& d = *status->owner;
    ++d.opens;
    d.draft = *initial->p;
    d.receivedMax = maximum;
    d.receivedSecret = secret;
    d.receivedFunc = function;
    d.receivedStatus = status;
    d.statusPendingAtDispatch = status->Int() == KRequestPending;
    return d.error;
}
std::u16string accepted;
void EHUIGetStoredText(int, int* length, const void* ptr) {
    *length = accepted.size();
    if (ptr) std::memcpy(const_cast<void*>(ptr), accepted.data(), accepted.size() * sizeof(char16_t));
}
void require(bool value, const char* what) { if (!value) throw std::runtime_error(what); }
void CheckRequest(const CHostBridgedImeFEP& f, const Editor& editor) {
    const auto& d = f.iImeDialog;
    require(d.opens == 1 && d.receivedFunc == 0, "wrong native request count/function");
    require(d.receivedMax == editor.maximum, "maximum length not forwarded");
    require(d.receivedSecret == (f.iInputCapabilities.rawSecret != 0 ? 1 : 0), "secret bit not normalized");
    require(d.receivedStatus == &d.iStatus && d.statusPendingAtDispatch, "status was not fifth argument/pending");
    require(d.error != KErrNone || d.active, "successful request not active");
}
