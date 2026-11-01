// Prepared normal-API canary only. No CommDB/DBMS API or setting writes.
// Execution remains gated on the complete native client/HLE storage-path proof.
#include <absl/base/nullability.h>
#include <e32base.h>
#include <es_sock.h>
#include <nifvar.h>
#include <stdio.h>

namespace {
struct Snapshot {
    TInt rc = KErrNotReady;
    TInt startRc = KErrNotReady;
    TInt iapRc = KErrNotReady;
    TInt networkRc = KErrNotReady;
    TInt serviceRc = KErrNotReady;
    TUint initialConnections = 0;
    TUint activeConnections = 0;
    bool initialEnumerated = false;
    bool startRequested = false;
    bool startCompleted = false;
    bool timedOut = false;
    bool progressConnected = false;
    bool iapAvailable = false;
    bool networkAvailable = false;
    bool serviceAvailable = false;
    bool stableSnapshot = false;
    bool ownedHandleClosed = false;
};
const char* absl_nonnull Bool(bool value) { return value ? "true" : "false"; }
void RecordError(Snapshot& out, TInt error) { if (out.rc == KErrNone && error) out.rc = error; }

class CStartOwner;
class CDeadline final : public CTimer {
public:
    // Match the public CBase new(ELeave) ownership for local destructors.
    static void operator delete(TAny* pointer) noexcept { User::Free(pointer); }
    static void operator delete(TAny* pointer, TLeave) noexcept { User::Free(pointer); }
    static CDeadline* absl_nonnull NewL(CStartOwner& owner);
    void Arm() { After(TTimeIntervalMicroSeconds32(5000000)); }
private:
    explicit CDeadline(CStartOwner& owner) : CTimer(EPriorityLow), iOwner(owner) { }
    void RunL() override;
    CStartOwner& iOwner;
};

class CStartOwner final : public CActive {
public:
    static void operator delete(TAny* pointer) noexcept { User::Free(pointer); }
    static void operator delete(TAny* pointer, TLeave) noexcept { User::Free(pointer); }
    static CStartOwner* absl_nonnull NewL(Snapshot& out) {
        CStartOwner* self = new (ELeave) CStartOwner(out);
        CleanupStack::PushL(self);
        self->iDeadline = CDeadline::NewL(*self);
        CleanupStack::Pop(self);
        return self;
    }
    ~CStartOwner() override {
        Cancel();
        CloseOwned();
        delete iDeadline;
    }
    bool Begin() {
        iOut.rc = iSockets.Connect();
        if (iOut.rc) return false;
        iSocketsOpen = true;
        iOut.rc = iConnection.Open(iSockets);
        if (iOut.rc) { CloseOwned(); return false; }
        iConnectionOpen = true;
        iOut.rc = iConnection.EnumerateConnections(iOut.initialConnections);
        if (iOut.rc) { CloseOwned(); return false; }
        iOut.initialEnumerated = true;
        if (iOut.initialConnections) {
            iOut.rc = KErrAlreadyExists; // Never take over an existing interface.
            CloseOwned();
            return false;
        }
        iOut.startRequested = true;
        iConnection.Start(iStatus); // OS default selection; no invented IAP.
        SetActive();
        iDeadline->Arm();
        return true;
    }
    void Expire() {
        iOut.timedOut = true;
        iOut.rc = KErrTimedOut;
        // Standard CActive::Cancel keeps iStatus alive while DoCancel closes
        // this owned subsession. No Stop, authoritative stop or guessed API.
        // Any stalled native IPC still requires the outer Root deadline gate.
        Cancel();
        CloseOwned();
        CActiveScheduler::Stop();
    }
private:
    explicit CStartOwner(Snapshot& out) : CActive(EPriorityStandard), iOut(out) {
        CActiveScheduler::Add(this);
    }
    void CloseOwned() {
        if (iConnectionOpen) {
            iConnection.Close();
            iConnectionOpen = false;
            iOut.ownedHandleClosed = true;
        }
        if (iSocketsOpen) {
            iSockets.Close();
            iSocketsOpen = false;
        }
    }
    void DoCancel() override { CloseOwned(); }
    bool Connected() {
        TNifProgress progress;
        TInt error = iConnection.Progress(progress);
        RecordError(iOut, error);
        if (error) return false;
        RecordError(iOut, progress.iError);
        return !progress.iError && progress.iStage != KConnectionUninitialised
            && progress.iStage != KConnectionDown;
    }
    void ReadSnapshot() {
        bool beforeConnected = Connected();
        TUint32 iap1 = 0, network1 = 0, service1 = 0;
        TUint32 iap2 = 0, network2 = 0, service2 = 0;
        iOut.iapRc = iConnection.GetIntSetting(_L("IAP\\Id"), iap1);
        iOut.networkRc = iConnection.GetIntSetting(_L("IAP\\IAPNetwork"), network1);
        iOut.serviceRc = iConnection.GetIntSetting(_L("IAP\\IAPService"), service1);
        TInt iapRc2 = iConnection.GetIntSetting(_L("IAP\\Id"), iap2);
        TInt networkRc2 = iConnection.GetIntSetting(_L("IAP\\IAPNetwork"), network2);
        TInt serviceRc2 = iConnection.GetIntSetting(_L("IAP\\IAPService"), service2);
        iOut.iapAvailable = !iOut.iapRc && !iapRc2 && iap1 && iap2;
        iOut.networkAvailable = !iOut.networkRc && !networkRc2;
        iOut.serviceAvailable = !iOut.serviceRc && !serviceRc2 && service1 && service2;
        TInt errors[] = { iOut.iapRc, iOut.networkRc, iOut.serviceRc, iapRc2, networkRc2, serviceRc2 };
        for (TInt error : errors)
            RecordError(iOut, error);
        TInt countError = iConnection.EnumerateConnections(iOut.activeConnections);
        RecordError(iOut, countError);
        bool afterConnected = Connected();
        iOut.progressConnected = beforeConnected && afterConnected;
        iOut.stableSnapshot = iOut.iapAvailable && iOut.networkAvailable && iOut.serviceAvailable
            && iap1 == iap2 && network1 == network2 && service1 == service2
            && iOut.progressConnected && !countError && iOut.activeConnections == 1;
        if (!iOut.stableSnapshot) RecordError(iOut, KErrNotReady);
    }
    void RunL() override {
        iDeadline->Cancel();
        iOut.startCompleted = true;
        iOut.startRc = iStatus.Int();
        iOut.rc = iOut.startRc;
        if (!iOut.rc) ReadSnapshot();
        CloseOwned();
        CActiveScheduler::Stop();
    }
    TInt RunError(TInt error) override {
        iDeadline->Cancel();
        iOut.rc = error;
        CloseOwned();
        CActiveScheduler::Stop();
        return KErrNone;
    }
    Snapshot& iOut;
    RSocketServ iSockets;
    RConnection iConnection;
    CDeadline* absl_nullable iDeadline = nullptr;
    bool iSocketsOpen = false;
    bool iConnectionOpen = false;
};

CDeadline* absl_nonnull CDeadline::NewL(CStartOwner& owner) {
    CDeadline* self = new (ELeave) CDeadline(owner);
    CleanupStack::PushL(self);
    self->ConstructL();
    CActiveScheduler::Add(self);
    CleanupStack::Pop(self);
    return self;
}
void CDeadline::RunL() { iOwner.Expire(); }

bool ReadToken(char* absl_nonnull token) {
    TBuf<80> command;
    TInt length = User::CommandLineLength();
    if (length < 14 || length > command.MaxLength()) return false;
    User::CommandLine(command);
    // EKA's opaque frontend argument keeps its leading separator verbatim.
    // Native RProcess callers need no separator; accept both transport forms.
    command.TrimLeft();
    _LIT(prefix, "--run ");
    if (command.Left(prefix().Length()) != prefix) return false;
    TPtrC value = command.Mid(prefix().Length());
    if (value.Length() < 8 || value.Length() > 64) return false;
    for (TInt i = 0; i < value.Length(); ++i) {
        TUint c = value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
        token[i] = static_cast<char>(c);
    }
    token[value.Length()] = 0;
    return true;
}
}

int main() {
    char token[65] = {};
    if (!ReadToken(token)) return 2;
    if (!freopen("C:/Data/WebKit/iap-owned-start.log", "w", stdout)) return 2;
    printf("BEGIN {\"suite\":\"iap-owned-start\",\"version\":1,\"run\":\"%s\"}\n", token);
    CTrapCleanup* cleanup = CTrapCleanup::New();
    if (!cleanup) { fclose(stdout); return 2; }
    Snapshot out;
    CActiveScheduler* scheduler = nullptr;
    TRAPD(error, {
        scheduler = new (ELeave) CActiveScheduler;
        CActiveScheduler::Install(scheduler);
        CStartOwner* owner = CStartOwner::NewL(out);
        CleanupStack::PushL(owner);
        if (owner->Begin()) CActiveScheduler::Start();
        CleanupStack::PopAndDestroy(owner);
    });
    if (error) out.rc = error;
    CActiveScheduler::Install(nullptr);
    delete scheduler;
    delete cleanup;
    printf("RESULT {\"suite\":\"iap-owned-start\",\"version\":1,\"run\":\"%s\","
        "\"status\":\"%s\",\"rc\":%d,\"start_rc\":%d,\"iap_rc\":%d,\"network_rc\":%d,\"service_rc\":%d,"
        "\"initial_connections\":%u,\"active_connections\":%u,"
        "\"initial_enumerated\":%s,\"start_requested\":%s,\"start_completed\":%s,\"timed_out\":%s,"
        "\"progress_connected\":%s,\"iap_available\":%s,\"network_available\":%s,\"service_available\":%s,"
        "\"stable_snapshot\":%s,\"owned_handle_closed\":%s}\n",
        token, out.rc == KErrNone ? "AVAILABLE" : "UNAVAILABLE", out.rc, out.startRc,
        out.iapRc, out.networkRc, out.serviceRc, out.initialConnections, out.activeConnections,
        Bool(out.initialEnumerated), Bool(out.startRequested), Bool(out.startCompleted), Bool(out.timedOut),
        Bool(out.progressConnected), Bool(out.iapAvailable), Bool(out.networkAvailable), Bool(out.serviceAvailable),
        Bool(out.stableSnapshot), Bool(out.ownedHandleClosed));
    printf("iap-owned-start exiting 0\n");
    fflush(stdout);
    fclose(stdout);
    // Zero is normal snapshot completion, including native API UNAVAILABLE.
    return 0;
}
