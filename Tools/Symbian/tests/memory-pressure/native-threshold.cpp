// EKA2L1's declared 8/24 MiB notification policy, exercised only by real commits.
// Never calls SetMemoryThresholds and never substitutes a synthetic notification.
#include "MemoryPressureMonitorSymbianNative.h"
#include <e32base.h>
#include <stdarg.h>
#include <stdio.h>

using namespace WTF::SymbianSystem;
static FILE* output;
static unsigned callbacks, lowCallbacks, freeCallbacks, lateCallbacks, errors;
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
    ++callbacks;
    lowCallbacks += !!(changes & memoryChangeLow);
    freeCallbacks += !!(changes & memoryChangeFree);
    lateCallbacks += !alive;
    errors += error != 0;
    log("NOTIFICATION flags=0x%x error=%d alive=%u", changes, error, alive);
}
class CDeadline final : public CTimer {
public:
    CDeadline() : CTimer(EPriorityStandard) { CActiveScheduler::Add(this); }
    static void operator delete(void* p) noexcept { User::Free(p); }
    void initL() { ConstructL(); }
    void RunL() override { CActiveScheduler::Stop(); }
};
static void wait(CDeadline& timer)
{
    timer.After(TTimeIntervalMicroSeconds32(500000));
    CActiveScheduler::Start();
}
static int boundedLogon(RChangeNotifier& notifier)
{
    RTimer timer;
    int error = timer.CreateLocal();
    if (error)
        return error;
    TRequestStatus timeout, status;
    timer.After(timeout, TTimeIntervalMicroSeconds32(500000));
    error = notifier.Logon(status);
    if (!error) {
        User::WaitForRequest(status, timeout);
        if (status == KRequestPending) {
            notifier.LogonCancel();
            User::WaitForRequest(status);
            error = KErrTimedOut;
        } else
            error = status.Int();
    }
    timer.Cancel();
    User::WaitForRequest(timeout);
    timer.Close();
    return error;
}
static void runL()
{
    constexpr int MiB = 1024 * 1024;
    constexpr int chunkSize = 32 * MiB;
    constexpr int step = 4 * MiB;
    constexpr int hardCap = 248 * MiB;
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
    wait(*timer);
    unsigned initialLow = lowCallbacks;
    RChangeNotifier coalesced;
    error = coalesced.Create();
    int initial = error ? error : boundedLogon(coalesced);
    log("INITIAL changes=%d low=%u", initial, initialLow);

    RChunk chunks[8];
    int sizes[8] = { };
    int total = 0;
    uint64_t before = 0, free = 0;
    bool valid = systemFreeMemory(before);
    free = before;
    bool accounting = valid && before >= 112U * MiB;
    log("HAL before valid=%u free=%llu hardCap=%d", valid, before, hardCap);
    while (accounting && !error && total < hardCap && free >= 8U * MiB) {
        int index = total / chunkSize;
        if (!sizes[index])
            error = chunks[index].CreateDisconnectedLocal(0, 0, chunkSize);
        if (error)
            break;
        uint64_t prior = free;
        error = chunks[index].Commit(sizes[index], step);
        if (error)
            break;
        unsigned char* bytes = chunks[index].Base();
        for (int offset = sizes[index]; offset < sizes[index] + step; offset += 4096) {
            bytes[offset] = static_cast<unsigned char>((index * 17 + offset / 4096) % 251 + 1);
            bytes[offset + 4095] = static_cast<unsigned char>(bytes[offset] ^ 0xA5);
        }
        sizes[index] += step;
        total += step;
        valid = systemFreeMemory(free);
        accounting = valid && prior >= free && prior - free >= step - 65536;
        log("COMMIT total=%d free=%llu error=%d accounting=%u", total, free, error, accounting);
    }
    wait(*timer);
    bool initialState = initial >= 0 && (initial & memoryChangeFree) && !(initial & memoryChangeLow) && !initialLow;
    bool low = initialState && accounting && !error && free < 8U * MiB && lowCallbacks > initialLow;
    bool pattern = total >= 80 * MiB;
    for (int index = 0; index < 8; ++index) {
        const unsigned char* bytes = chunks[index].Base();
        for (int offset = 0; offset < sizes[index]; offset += 4096) {
            unsigned char expected = static_cast<unsigned char>((index * 17 + offset / 4096) % 251 + 1);
            pattern &= bytes[offset] == expected && bytes[offset + 4095] == (expected ^ 0xA5);
        }
    }
    log("LOW reached=%u free=%llu live=%d pattern=%u", low, free, total, pattern);
    // Release only the tail. At least 80 MiB of original live pages remain.
    unsigned recoveryBefore = freeCallbacks;
    int tailIndex = -1, tailOffset = 0, tailSize = 0;
    if (total >= 112 * MiB) {
        for (int index = 7; index >= 0; --index) {
            if (sizes[index] >= 24 * MiB) {
                tailIndex = index;
                tailSize = 24 * MiB;
                tailOffset = sizes[index] - tailSize;
                chunks[index].Decommit(tailOffset, tailSize);
                sizes[index] -= tailSize;
                total -= tailSize;
                break;
            }
        }
    }
    wait(*timer);
    valid = systemFreeMemory(free);
    bool recovery = valid && free >= 24U * MiB && freeCallbacks > recoveryBefore;
    int combined = initial >= 0 ? boundedLogon(coalesced) : initial;
    bool coalescing = combined >= 0 && (combined & 0xA0) == 0xA0;
    coalesced.Close();
    for (int index = 0; index < 8; ++index) {
        const unsigned char* bytes = chunks[index].Base();
        for (int offset = 0; offset < sizes[index]; offset += 4096) {
            unsigned char expected = static_cast<unsigned char>((index * 17 + offset / 4096) % 251 + 1);
            pattern &= bytes[offset] == expected && bytes[offset + 4095] == (expected ^ 0xA5);
        }
    }
    log("RECOVERY free=%llu live=%d recovery=%u coalesced=0x%x pattern=%u", free, total, recovery, combined, pattern);
    destroyMemoryPressureMonitor(monitor);
    alive = false;
    unsigned cancelledAt = callbacks;
    // Cross low/good once more after cancellation; no callback may reach its owner.
    if (tailIndex >= 0)
        error = chunks[tailIndex].Commit(tailOffset, tailSize);
    for (auto& chunk : chunks)
        chunk.Close();
    wait(*timer);
    log("CANCEL before=%u after=%u late=%u errors=%u recommitError=%d", cancelledAt, callbacks, lateCallbacks, errors, error);
    bool pass = accounting && low && recovery && coalescing && pattern && total >= 80 * MiB
        && !error && !errors && !lateCallbacks && callbacks == cancelledAt;
    log("PROBE_RESULT accounting=%s low=%s recovery=%s coalescing=%s live80=%s cancellation=%s",
        accounting ? "PASS" : "FAIL", low ? "PASS" : "FAIL", recovery ? "PASS" : "FAIL",
        coalescing ? "PASS" : "FAIL", pattern && total >= 80 * MiB ? "PASS" : "FAIL",
        callbacks == cancelledAt && !lateCallbacks && !errors ? "PASS" : "FAIL");
    CleanupStack::PopAndDestroy(timer);
    CActiveScheduler::Install(nullptr);
    if (!pass)
        User::Leave(KErrGeneral);
}
int main()
{
    output = fopen("C:/Data/memory-pressure-threshold.log", "w");
    if (!output)
        return 2;
    setvbuf(output, nullptr, _IONBF, 0);
    TRAPD(error, runL());
    log("EXIT error=%d", error);
    fclose(output);
    return error ? 1 : 0;
}
