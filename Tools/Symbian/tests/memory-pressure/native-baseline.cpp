#include "MemoryPressureMonitorSymbianNative.h"
#include <e32base.h>
#include <stdio.h>
#include <stdarg.h>

using namespace WTF::SymbianSystem;
static FILE* output;
static unsigned notifications;
static unsigned memoryNotifications;
static unsigned callbackErrors;
static unsigned lateCallbacks;
static bool alive;
static void log(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(output, format, args);
    va_end(args);
    fputc('\n', output);
    fflush(output);
}
static void changed(void*, unsigned changes, int error)
{
    ++notifications;
    if (changes & (memoryChangeFree | memoryChangeLow))
        ++memoryNotifications;
    if (error)
        ++callbackErrors;
    if (!alive)
        ++lateCallbacks;
    log("NOTIFICATION flags=0x%x error=%d alive=%u", changes, error, alive);
}
class CDeadline final : public CTimer {
public:
    CDeadline() : CTimer(EPriorityStandard) { CActiveScheduler::Add(this); }
    static void operator delete(void* pointer) noexcept { User::Free(pointer); }
    void initL() { ConstructL(); }
    void RunL() override { CActiveScheduler::Stop(); }
};
static void wait(CDeadline& timer, int microseconds)
{
    timer.After(TTimeIntervalMicroSeconds32(microseconds));
    CActiveScheduler::Start();
}
static void runL()
{
    CActiveScheduler scheduler;
    CActiveScheduler::Install(&scheduler);
    auto* timer = new (ELeave) CDeadline;
    CleanupStack::PushL(timer);
    timer->initL();
    int error = 0;
    alive = true;
    void* monitor = createMemoryPressureMonitor(changed, nullptr, error);
    log("CREATE monitor=%u error=%d", monitor != nullptr, error);
    if (!monitor)
        User::Leave(error);
    wait(*timer, 500000);
    uint64_t before = 0, reserved = 0, committed = 0, recovered = 0;
    bool validBefore = systemFreeMemory(before);
    log("HAL before valid=%u free=%llu", validBefore, before);
    const int size = 64 * 1024 * 1024;
    RChunk chunk;
    error = chunk.CreateDisconnectedLocal(0, 0, size);
    bool validReserved = systemFreeMemory(reserved);
    log("RESERVE bytes=%d error=%d freeValid=%u free=%llu", size, error, validReserved, reserved);
    bool attempted = validBefore && before >= static_cast<uint64_t>(size + 32 * 1024 * 1024);
    if (!error && attempted) {
        error = chunk.Commit(0, size);
        if (!error) {
            volatile unsigned char* bytes = chunk.Base();
            for (int offset = 0; offset < size; offset += 4096)
                bytes[offset] = static_cast<unsigned char>((offset / 4096) % 251 + 1);
        }
    }
    bool validCommitted = systemFreeMemory(committed);
    log("COMMIT attempted=%u bytes=%d error=%d freeValid=%u free=%llu", attempted, size, error, validCommitted, committed);
    wait(*timer, 2000000);
    bool pattern = attempted && error == KErrNone;
    if (pattern) {
        const unsigned char* bytes = chunk.Base();
        for (int offset = 0; offset < size; offset += 4096)
            pattern &= bytes[offset] == static_cast<unsigned char>((offset / 4096) % 251 + 1);
    }
    chunk.Close();
    wait(*timer, 1000000);
    bool validRecovered = systemFreeMemory(recovered);
    log("RELEASE freeValid=%u free=%llu pattern=%u", validRecovered, recovered, pattern);
    destroyMemoryPressureMonitor(monitor);
    alive = false;
    unsigned completedAtCancel = notifications;
    wait(*timer, 500000);
    log("CANCEL completed=%u after=%u late=%u errors=%u", completedAtCancel, notifications, lateCallbacks, callbackErrors);
    bool accounting = validBefore && validReserved && validCommitted && validRecovered && attempted && !error
        && before >= committed && before - committed >= 48U * 1024 * 1024
        && recovered >= committed && recovered - committed >= 48U * 1024 * 1024;
    log("PROBE_RESULT bridge=%s pattern=%s accounting=%s memoryNotifications=%u", !lateCallbacks && !callbackErrors ? "PASS" : "FAIL", pattern ? "PASS" : "FAIL", accounting ? "PASS" : "UNAVAILABLE", memoryNotifications);
    log("NOTE no notification is required unless a real system threshold was crossed; no threshold was changed");
    CleanupStack::PopAndDestroy(timer);
    CActiveScheduler::Install(nullptr);
}
int main()
{
    output = fopen("C:/Data/memory-pressure-probe.log", "w");
    if (!output)
        return 2;
    setvbuf(output, nullptr, _IONBF, 0);
    TRAPD(error, runL());
    log("EXIT error=%d", error);
    fclose(output);
    return error ? 1 : 0;
}
