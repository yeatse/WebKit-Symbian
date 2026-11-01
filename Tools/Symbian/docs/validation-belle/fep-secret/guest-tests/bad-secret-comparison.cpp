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
TInt CHostDialogIme::Open(const TDesC *aInitialText, const TInt aMaxLength, const TBool aSecret) {
    iStatus = KRequestPending;
    TInt result = ::EHUIOpenGlobalTextViewWithSecret(0, aInitialText, aMaxLength, aSecret, &iStatus);
    if (result != KErrNone) {
        return result;
    }
    
    SetActive();
    return KErrNone;
}
void CHostDialogIme::RunL() {
    if (iStatus.Int() == KErrNone)
        iHost->CommitChangedTextL();
    else
        iHost->CancelChangedTextL();
}
void CHostBridgedImeFEP::OpenDialogInputL(const TBool aTranstractRestart) {
    MCoeFepAwareTextEditor *editor = iInputCapabilities.FepAwareTextEditor();
    
    HBufC *initialText = NULL;
    TPtr initialTextPtr(0, 0);

    if (editor->DocumentLengthForFep()) {
        // Spawn text dialog from host and listen for completion
        initialText = HBufC::NewLC(editor->DocumentLengthForFep());
        initialTextPtr.Set(initialText->Des());
        initialTextPtr.SetLength(editor->DocumentLengthForFep());

        editor->GetEditorContentForFep(initialTextPtr, 0, editor->DocumentLengthForFep());
    }

    // The host dialog owns its draft. Opening/cancelling it must not replace
    // the field's selection or start a composition in the native editor.
    // Begin the whole-document replacement only after an accepted completion.
    iInFEPWork = ETrue;
    // SupportsSecretText returns a TBool bit mask (0x100), not necessarily 1.
    // Capture and normalize the current editor's mode for this request only.
    const TBool secret = iInputCapabilities.SupportsSecretText() == ETrue;
    TInt error = iImeDialog.Open(&initialTextPtr, editor->DocumentMaximumLengthForFep(), secret);
    iInFEPWork = EFalse;

    if (initialText) {
        CleanupStack::PopAndDestroy();
    }
    User::LeaveIfError(error);
}
void CHostBridgedImeFEP::CommitChangedTextL() {
    MCoeFepAwareTextEditor *editor = iInputCapabilities.FepAwareTextEditor();
    if (!editor) {
        LogOut(KAvkonFepCat, _L("Trying to commit while FEP Aware editor is not available (impossible)!"));
        return;
    }

    if (!iDialogPending) {
        return;
    }

    iDialogPending = EFalse;
    
    TInt length = 0;
    ::EHUIGetStoredText(0, &length, NULL);

    TPtr textChanged(0, 0);
    HBufC *textChangedBuf = NULL;
    
    if (length != 0) {
        textChangedBuf = HBufC::NewLC(length);
        textChanged.Set(textChangedBuf->Des());
        textChanged.SetLength(length);
        
        ::EHUIGetStoredText(0, &length, textChangedBuf->Ptr());
    }

    TCursorSelection wholeDocument(0, editor->DocumentLengthForFep());
    editor->SetCursorSelectionForFepL(wholeDocument);
    editor->StartFepInlineEditL(textChanged, textChanged.Length(), ETrue, NULL, *this, *this);
    editor->CommitFepInlineEditL(*CCoeEnv::Static());

    TInt endPosition = editor->DocumentLengthForFep();
    TCursorSelection selection(endPosition, endPosition);
    editor->SetCursorSelectionForFepL(selection);

    if (textChangedBuf) {
        CleanupStack::PopAndDestroy();
    }
}
void CHostBridgedImeFEP::CancelChangedTextL() {
    // No inline edit was started while the host draft was pending. Preserve
    // both text and the native editor's selection, including an empty field.
    iDialogPending = EFalse;
}
void CHostBridgedImeFEP::CancelDialogL() {
    if (iDialogPending) {
        iDialogPending = EFalse;
        iImeDialog.Cancel();
    }
}
int main(){int cases=0;try{
for(int rawSecret : {0, 0x100}) { for(int maximum : {64, 1024*1024}) {
 for(auto original : {std::u16string{},std::u16string(u"A|B"),std::u16string(u"known fixture \u4f60\u597d \U0001f600\n")}){
  for(bool backwards:{false,true}){
   Editor editor;editor.maximum=maximum;editor.text=original;editor.anchor=backwards?2:1;editor.cursor=backwards?1:2;
   if(original.empty())editor.anchor=editor.cursor=0;
   auto anchor=editor.anchor,cursor=editor.cursor;CHostBridgedImeFEP f;f.iInputCapabilities.editor=&editor;f.iInputCapabilities.rawSecret=rawSecret;f.iImeDialog.iHost=&f;f.OpenDialogInputL();CheckRequest(f,editor);f.iDialogPending=true;
   require(editor.text==original&&editor.anchor==anchor&&editor.cursor==cursor,"open changed field/selection");require(!editor.starts&&!editor.sets,"open began edit");require(f.iImeDialog.draft==original,"host draft mismatch");
   f.iImeDialog.iStatus.value=KErrCancel;f.iImeDialog.RunL();require(!f.iDialogPending,"cancel retained pending");require(editor.text==original&&editor.anchor==anchor&&editor.cursor==cursor,"cancel changed field/selection");require(!editor.starts&&!editor.sets&&!editor.commits,"cancel edited field");++cases;
  }
 }
 for(auto replacement : {std::u16string{},std::u16string(u"accepted \u4f60\u597d \U0001f600\n")}){
  Editor editor;editor.maximum=maximum;editor.text=u"old known fixture";editor.anchor=2;editor.cursor=4;CHostBridgedImeFEP f;f.iInputCapabilities.editor=&editor;f.iInputCapabilities.rawSecret=rawSecret;f.iImeDialog.iHost=&f;f.OpenDialogInputL();CheckRequest(f,editor);f.iDialogPending=true;accepted=replacement;f.iImeDialog.iStatus.value=KErrNone;f.iImeDialog.RunL();require(editor.text==replacement,"accepted text not replaced");require(editor.starts==1&&editor.commits==1&&!f.iDialogPending,"accepted completion mismatch");require(editor.anchor==(int)replacement.size()&&editor.cursor==(int)replacement.size(),"accepted caret mismatch");f.iImeDialog.RunL();require(editor.commits==1,"duplicate completion committed twice");++cases;
 }
 {Editor editor;editor.maximum=maximum;editor.text=u"unchanged";editor.anchor=2;editor.cursor=3;CHostBridgedImeFEP f;f.iInputCapabilities.editor=&editor;f.iInputCapabilities.rawSecret=rawSecret;f.OpenDialogInputL();CheckRequest(f,editor);f.iDialogPending=true;f.CancelDialogL();require(editor.text==u"unchanged"&&editor.anchor==2&&editor.cursor==3,"guest close edited field");require(!f.iDialogPending&&f.iImeDialog.cancels==1,"guest close mismatch");++cases;}
 {Editor editor;editor.maximum=maximum;editor.text=u"open failure";editor.anchor=3;editor.cursor=2;CHostBridgedImeFEP f;f.iInputCapabilities.editor=&editor;f.iInputCapabilities.rawSecret=rawSecret;f.iImeDialog.error=-14;bool failed=false;try{f.OpenDialogInputL();CheckRequest(f,editor);}catch(int e){failed=e==-14;}require(failed&&!f.iInFEPWork,"open failure state");require(editor.text==u"open failure"&&editor.anchor==3&&editor.cursor==2&&!editor.starts,"open failure edited field");++cases;}
 } }
 require(oldA0Calls==0,"legacy A0 was used");
 require(CleanupStack::stack.empty(),"cleanup stack leaked");std::cout<<cases<<" actual-method state cases PASS (secret 0/256, maxima 64/1048576; mock editor; no guest)\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}return 0;}
