#include "../../../../../Source/WebCore/platform/symbian/NetworkStateNotifierSymbianNative.h"
#include <e32base.h>
#include <es_sock.h>
#define SYMBIAN_ENABLE_SPLIT_HEADERS
#include "official/es_enum.h"
#include <nifvar.h>
#include <rconnmon.h>
#include <stdio.h>

namespace Native = WebCore::SymbianNetworkState;
static unsigned passed;
static unsigned failed;
static unsigned changes;

static void check(bool success, const char* label)
{
    printf("%s: %s\n", success ? "PASS" : "FAIL", label);
    fflush(stdout);
    success ? ++passed : ++failed;
}

class Slice final : public CTimer {
public:
    Slice() : CTimer(EPriorityStandard) { CActiveScheduler::Add(this); }
    void initializeL() { ConstructL(); }
    void pump()
    {
        After(TTimeIntervalMicroSeconds32(500000));
        CActiveScheduler::Start();
    }
private:
    void RunL() override { CActiveScheduler::Stop(); }
};

class Events final : public MConnectionMonitorObserver {
public:
    void EventL(const CConnMonEventBase& event) override
    {
        ++total;
        if (event.EventType() == EConnMonCreateConnection)
            ++created;
        if (event.EventType() == EConnMonConnectionStatusChange) {
            auto status = static_cast<const CConnMonConnectionStatusChange&>(event).ConnectionStatus();
            if (status == KLinkLayerOpen)
                ++opened;
        }
        if (event.EventType() == EConnMonDeleteConnection) {
            ++deleted;
            if (static_cast<const CConnMonDeleteConnection&>(event).AuthoritativeDelete())
                ++authoritative;
        }
    }
    unsigned total = 0;
    unsigned created = 0;
    unsigned opened = 0;
    unsigned deleted = 0;
    unsigned authoritative = 0;
};

struct Handles {
    ~Handles()
    {
        Native::destroyObserver(observer);
        monitor.CancelNotifications();
        monitor.Close();
        attached.Close();
        connection.Close();
        sockets.Close();
    }
    RSocketServ sockets;
    RConnection connection;
    RConnection attached;
    RConnectionMonitor monitor;
    void* observer = nullptr;
};

static void runL()
{
    Slice slice;
    slice.initializeL();
    Events events;
    Handles handles;
    User::LeaveIfError(handles.sockets.Connect());
    User::LeaveIfError(handles.monitor.ConnectL());
    User::LeaveIfError(handles.monitor.NotifyEventL(events));
    handles.observer = Native::createObserver([](void*) { ++changes; }, nullptr);
    check(handles.observer, "production native observer created");
    if (!handles.observer)
        return;
    bool online = true;
    check(!Native::state(handles.observer, online), "initial state is asynchronous and unknown");
    slice.pump();
    check(Native::state(handles.observer, online) && !online && changes == 1,
        "idle guest has no active connection");
    if (online)
        return; // Other guest clients must be closed for this destructive Stop probe.

    User::LeaveIfError(handles.connection.Open(handles.sockets));
    User::LeaveIfError(handles.connection.Start());
    slice.pump();
    check(Native::state(handles.observer, online) && online && changes == 2,
        "RConnection Start publishes online");
    check(events.created == 1 && events.opened == 1, "ConnMon sends create and link-open event payloads");
    TUint count = 0;
    TRequestStatus request;
    handles.monitor.GetConnectionCount(count, request);
    User::WaitForRequest(request);
    check(request.Int() == KErrNone && count == 1, "ConnMon snapshots one actual interface");
    TUint id = 0;
    TUint subcount = 123;
    check(handles.monitor.GetConnectionInfo(1, id, subcount) == KErrNone && id && id < 2000000 && !subcount,
        "connection ID is distinct from bearer IDs");
    TInt stage = 0;
    handles.monitor.GetIntAttribute(id, 0, KConnectionStatus, stage, request);
    User::WaitForRequest(request);
    check(request.Int() == KErrNone && stage == KLinkLayerOpen, "connection status is the real link-open stage");
    check(handles.monitor.GetConnectionInfo(0, id, subcount) == KErrArgument, "zero enumeration index rejected");

    User::LeaveIfError(handles.connection.EnumerateConnections(count));
    TPckgBuf<TConnectionInfo> info;
    User::LeaveIfError(handles.connection.GetConnectionInfo(1, info));
    User::LeaveIfError(handles.attached.Open(handles.sockets));
    User::LeaveIfError(handles.attached.Attach(info, RConnection::EAttachTypeNormal));
    handles.connection.Close();
    slice.pump();
    check(Native::state(handles.observer, online) && online && changes == 2,
        "normal attachment keeps shared interface online");
    handles.attached.Close();
    slice.pump();
    check(Native::state(handles.observer, online) && !online && changes == 3,
        "last owner close publishes offline");
    check(events.deleted == 1 && !events.authoritative, "normal close deletion is not authoritative");

    User::LeaveIfError(handles.connection.Open(handles.sockets));
    User::LeaveIfError(handles.connection.Start());
    slice.pump();
    check(Native::state(handles.observer, online) && online && changes == 4, "reopen reconnects");
    User::LeaveIfError(handles.connection.Stop(RConnection::EStopAuthoritative));
    slice.pump();
    check(Native::state(handles.observer, online) && !online && changes == 5, "explicit Stop publishes offline");
    check(events.deleted == 2 && events.authoritative == 1, "authoritative Stop flag survives event IPC");
    User::LeaveIfError(handles.connection.Start());
    slice.pump();
    check(Native::state(handles.observer, online) && online && changes == 6, "Start after Stop reconnects");

    handles.monitor.CancelNotifications();
    auto notifications = events.total;
    User::LeaveIfError(handles.connection.Stop());
    slice.pump();
    check(Native::state(handles.observer, online) && !online && changes == 7,
        "another monitor cancellation does not stop native observer");
    check(events.total == notifications, "cancelled ConnMon receives no later callbacks");
    Native::destroyObserver(handles.observer);
    handles.observer = nullptr;
    auto before = changes;
    User::LeaveIfError(handles.connection.Start());
    slice.pump();
    check(changes == before, "destroyed observer receives no callbacks");
    handles.connection.Stop();
    slice.pump();
}

int main()
{
    if (!freopen("C:/Data/WebKit/network-state-probe.log", "w", stdout))
        return 2;
    printf("BEGIN: native-network-state version=1\n");
    CTrapCleanup* cleanup = CTrapCleanup::New();
    CActiveScheduler* scheduler = new CActiveScheduler;
    if (!cleanup || !scheduler)
        return 2;
    CActiveScheduler::Install(scheduler);
    TRAPD(error, runL());
    if (error) {
        printf("FAIL: native leave %d\n", error);
        ++failed;
    }
    printf("RESULT: passed=%u failed=%u\n", passed, failed);
    printf("network-state-probe exiting %d\n", failed ? 1 : 0);
    fflush(stdout);
    CActiveScheduler::Install(nullptr);
    delete scheduler;
    delete cleanup;
    return failed ? 1 : 0;
}
