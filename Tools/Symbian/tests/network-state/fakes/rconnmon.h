#pragma once
#include "e32base.h"
#include <functional>
#include <utility>

constexpr TInt EConnMonGetConnectionCount = 1;
constexpr TInt EConnMonGetIntAttribute = 2;
constexpr TUint KConnectionStatus = 3;
enum { EConnMonCreateConnection = 1, EConnMonDeleteConnection = 2, EConnMonNetworkStatusChange = 7, EConnMonConnectionStatusChange = 8, EConnMonBearerChange = 11 };
class CConnMonEventBase {
public:
    explicit CConnMonEventBase(TInt type) : type(type) { }
    TInt EventType() const { return type; }
private:
    TInt type;
};
class MConnectionMonitorObserver {
public:
    virtual void EventL(const CConnMonEventBase&) = 0;
};
namespace FakeMonitor {
inline std::vector<TInt> connections;
inline MConnectionMonitorObserver* observer = nullptr;
inline std::function<void()> pending;
inline TInt pendingKind = 0;
inline TInt connectError = 0;
inline TInt queryError = 0;
inline bool badInfo = false;
inline TUint countOverride = 0;
inline unsigned cancellations = 0;
inline unsigned closed = 0;
inline unsigned unsubscribed = 0;
inline void event(TInt type = EConnMonConnectionStatusChange) { assert(observer); observer->EventL(CConnMonEventBase(type)); }
inline void one() { assert(pending); auto completion = std::exchange(pending, {}); pendingKind = 0; completion(); }
inline void drain() { unsigned limit = 0; while (pending) { assert(++limit < 100); one(); } }
}
class RConnectionMonitor {
public:
    TInt ConnectL() { return FakeMonitor::connectError; }
    TInt NotifyEventL(MConnectionMonitorObserver& observer) { FakeMonitor::observer = &observer; return KErrNone; }
    void CancelNotifications() { FakeMonitor::observer = nullptr; ++FakeMonitor::unsubscribed; }
    void Close() { ++FakeMonitor::closed; }
    void CancelAsyncRequest(TInt kind) {
        assert(FakeMonitor::pending && FakeMonitor::pendingKind == kind);
        FakeMonitor::pending = {};
        FakeMonitor::pendingKind = 0;
        ++FakeMonitor::cancellations;
    }
    void GetConnectionCount(TUint& count, TRequestStatus& status) {
        assert(!FakeMonitor::pending);
        FakeMonitor::pendingKind = EConnMonGetConnectionCount;
        FakeMonitor::pending = [&count, &status] {
            count = FakeMonitor::countOverride ? FakeMonitor::countOverride : FakeMonitor::connections.size();
            FakeKernel::complete(&status, FakeMonitor::queryError);
        };
    }
    TInt GetConnectionInfo(TUint index, TUint& connection, TUint& subcount) {
        if (FakeMonitor::badInfo || !index || index > FakeMonitor::connections.size()) return KErrNotFound;
        connection = index; subcount = 0; return KErrNone;
    }
    void GetIntAttribute(TUint connection, TUint subconnection, TUint attribute, TInt& value, TRequestStatus& status) {
        assert(!FakeMonitor::pending && !subconnection && attribute == KConnectionStatus);
        FakeMonitor::pendingKind = EConnMonGetIntAttribute;
        FakeMonitor::pending = [connection, &value, &status] {
            if (!connection || connection > FakeMonitor::connections.size()) { FakeKernel::complete(&status, KErrNotFound); return; }
            value = FakeMonitor::connections[connection - 1];
            FakeKernel::complete(&status, FakeMonitor::queryError);
        };
    }
};
