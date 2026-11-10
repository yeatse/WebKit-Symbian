#include "MemoryPressureMonitorSymbianNative.h"
#include <e32base.h>
#include <hal.h>
#include <cstdio>
#include <cstdlib>
using namespace WTF::SymbianSystem;
static unsigned calls, changes;
static int callbackError;
static void callback(void*, unsigned value, int error) { ++calls; changes = value; callbackError = error; }
static unsigned checks;
static void expect(bool value) { ++checks; if (!value) { std::fprintf(stderr, "native check %u failed\n", checks); std::abort(); } }
int main()
{
    int error;
    NativeTest::scheduler = false;
    expect(!createMemoryPressureMonitor(callback, nullptr, error) && error == KErrNotReady);
    NativeTest::scheduler = true;
    NativeTest::createError = -4;
    expect(!createMemoryPressureMonitor(callback, nullptr, error) && error == -4);
    expect(!NativeTest::handles && !NativeTest::allocations);
    NativeTest::createError = 0;
    NativeTest::logonError = -8;
    expect(!createMemoryPressureMonitor(callback, nullptr, error) && error == -8);
    expect(!NativeTest::handles && !NativeTest::allocations);
    NativeTest::logonError = 0;
    auto* monitor = createMemoryPressureMonitor(callback, nullptr, error);
    expect(monitor && !error && NativeTest::handles == 1);
    int logons = NativeTest::logons;
    NativeTest::observer->complete(EChangesLowMemory);
    expect(calls == 1 && changes == EChangesLowMemory && !callbackError && NativeTest::logons == logons + 1);
    destroyMemoryPressureMonitor(monitor);
    expect(NativeTest::cancels == 1 && !NativeTest::handles && !NativeTest::allocations);
    monitor = createMemoryPressureMonitor(callback, nullptr, error);
    NativeTest::logonError = -9;
    NativeTest::observer->complete(EChangesFreeMemory);
    expect(calls == 2 && changes == EChangesFreeMemory && callbackError == -9);
    destroyMemoryPressureMonitor(monitor);
    expect(NativeTest::cancels == 1 && !NativeTest::handles && !NativeTest::allocations);
    NativeTest::logonError = 0;
    monitor = createMemoryPressureMonitor(callback, nullptr, error);
    NativeTest::observer->complete(-6);
    expect(calls == 3 && !changes && callbackError == -6);
    destroyMemoryPressureMonitor(monitor);
    expect(NativeTest::cancels == 1 && !NativeTest::handles && !NativeTest::allocations);
    uint64_t free = 999;
    expect(systemFreeMemory(free) && free == 100);
    NativeTest::halError = -5; free = 999;
    expect(!systemFreeMemory(free) && free == 999);
    NativeTest::halError = 0; NativeTest::freeRAM = -1;
    expect(!systemFreeMemory(free) && free == 999);
    NativeTest::freeRAM = 0;
    expect(systemFreeMemory(free) && free == 0);
    destroyMemoryPressureMonitor(nullptr);
    std::printf("PASS actual native bridge with SDK mocks: %u lifecycle/error checks\n", checks);
}
