// Compile the production native observer, replacing only the OS API boundary.
#include "NetworkStateNotifierSymbianNative.h"
#include "rconnmon.h"
#include "nifvar.h"
#include <cstdio>

namespace Native = WebCore::SymbianNetworkState;
static unsigned checks;
static void check(bool condition) { assert(condition); ++checks; }
static void changed(void* count) { ++*static_cast<unsigned*>(count); }

int main()
{
    FakeKernel::scheduler = false;
    check(!Native::createObserver(nullptr, nullptr));
    FakeKernel::scheduler = true;
    unsigned events = 0;
    void* observer = Native::createObserver(changed, &events);
    bool online = true;
    check(observer && !Native::state(observer, online) && online);
    FakeMonitor::drain();
    check(Native::state(observer, online) && !online && events == 1);
    FakeMonitor::event(); FakeMonitor::drain();
    check(events == 1); // Repeated disconnected snapshots do not duplicate events.
    FakeMonitor::connections = { KLinkLayerOpen };
    FakeMonitor::event(EConnMonCreateConnection); FakeMonitor::drain();
    check(Native::state(observer, online) && online && events == 2);
    FakeMonitor::connections = { KConnectionClosed, KConnectionOpen };
    FakeMonitor::event(); FakeMonitor::drain();
    check(Native::state(observer, online) && online && events == 2);
    FakeMonitor::connections = { 4000 }; // Suspended/unknown is not a disconnect.
    FakeMonitor::event(); FakeMonitor::drain();
    check(Native::state(observer, online) && online && events == 2);
    FakeMonitor::badInfo = true;
    FakeMonitor::event(); FakeMonitor::drain();
    check(Native::state(observer, online) && online && events == 2);
    FakeMonitor::badInfo = false;
    FakeMonitor::queryError = -15;
    FakeMonitor::event(); FakeMonitor::drain();
    check(Native::state(observer, online) && online && !FakeMonitor::observer && events == 2);
    FakeMonitor::queryError = 0;
    FakeMonitor::connections.clear();
    check(FakeKernel::fireTimer() == 5000000);
    FakeMonitor::one(); // Count zero, then publish offline.
    check(Native::state(observer, online) && !online && events == 3);
    FakeMonitor::connections = { KLinkLayerOpen };
    FakeMonitor::event();
    FakeMonitor::one(); // Count completes; status query is in flight.
    FakeMonitor::connections.clear();
    FakeMonitor::event(EConnMonDeleteConnection);
    // The deleted connection fails the status query. It must not publish a
    // fabricated new online value; retry establishes the current empty set.
    FakeMonitor::drain();
    check(!online && events == 3);
    FakeKernel::fireTimer(); FakeMonitor::drain();
    check(Native::state(observer, online) && !online && events == 3);
    FakeMonitor::event();
    FakeMonitor::connections = { KLinkLayerOpen };
    FakeMonitor::event(EConnMonCreateConnection); // Invalidates the pending count.
    FakeMonitor::one(); FakeMonitor::one();
    check(events == 3 && FakeMonitor::pending); // Stale completed snapshot discarded.
    FakeMonitor::drain();
    check(Native::state(observer, online) && online && events == 4);
    FakeMonitor::event();
    auto cancelled = FakeMonitor::cancellations;
    check(FakeKernel::fireTimer() == 5000000);
    check(FakeMonitor::cancellations == cancelled + 1 && !FakeMonitor::pending && events == 4);
    FakeMonitor::countOverride = 1025;
    FakeKernel::fireTimer(); FakeMonitor::drain();
    check(Native::state(observer, online) && online && events == 4);
    FakeMonitor::countOverride = 0;
    FakeKernel::fireTimer(); FakeMonitor::one();
    cancelled = FakeMonitor::cancellations;
    Native::destroyObserver(observer);
    check(FakeMonitor::cancellations == cancelled + 1 && !FakeMonitor::pending && !FakeMonitor::observer && FakeKernel::activeObjects.empty());

    FakeMonitor::connectError = -18;
    observer = Native::createObserver(changed, &events);
    check(observer && !Native::state(observer, online) && !FakeMonitor::pending);
    FakeMonitor::connectError = 0;
    FakeKernel::fireTimer(); FakeMonitor::drain();
    check(Native::state(observer, online) && online && events == 5);
    Native::destroyObserver(observer);
    check(FakeKernel::activeObjects.empty());

    observer = Native::createObserver([](void* context) {
        auto** observer = static_cast<void**>(context);
        Native::destroyObserver(*observer);
        *observer = nullptr;
    }, &observer);
    FakeMonitor::drain();
    check(!observer && !FakeMonitor::pending && !FakeMonitor::observer && FakeKernel::activeObjects.empty());
    check(User::frees == 6); // Three observers and their native timers use User::Free.
    std::printf("PASS: %u production native observer checks\n", checks);
}
