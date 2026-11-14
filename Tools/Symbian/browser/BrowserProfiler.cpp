#include "BrowserProfiler.h"

#include "BrowserLog.h"

#include <e32base.h>
#include <e32std.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// JavaScriptCore's (jit/ExecutableAllocator.h), whose headers the browser
// does not otherwise include.
namespace JSC {
void* startOfFixedExecutableMemoryPoolImpl();
void* endOfFixedExecutableMemoryPoolImpl();
}

namespace {

// What a table entry counts, in the low bits of its key.
enum Kind : uint32_t {
    Image = 0, // The program counter, in this executable.
    Jit = 1, // In JIT code, by 4 KB.
    Foreign = 2, // In a system library or the kernel's user-side stubs.
    ForeignCaller = 3, // The link register of a Foreign sample, in this executable.
};

struct Entry {
    uint32_t key;
    uint32_t count;
};

const unsigned KTableSize = 1 << 15;
const unsigned KBucketShift = 5;
const TInt KPeriodMicroseconds = 5000;
const unsigned KMaximumSamples = 10 * 60 * 200;
const unsigned KSamplesPerWrite = 30 * 200;

Entry* table;
Entry* sorted;
unsigned samplesWritten;
unsigned tableUsed;
unsigned samplesBusy;
unsigned samplesIdle;
unsigned samplesDropped;
unsigned samplesFailed;
unsigned kindCounts[4];
uintptr_t imageBase;
uintptr_t imageEnd;
TThreadId uiThreadId;
volatile bool stopRequested;
volatile bool stopped;

void count(uint32_t key)
{
    uint32_t index = (key * 2654435761u) >> (32 - 15);
    for (unsigned probes = 0; probes < 64; ++probes, index = (index + 1) & (KTableSize - 1)) {
        Entry& entry = table[index];
        if (entry.key == key) {
            ++entry.count;
            return;
        }
        if (!entry.count) {
            entry.key = key;
            entry.count = 1;
            ++tableUsed;
            return;
        }
    }
    ++samplesDropped;
}

void sample(RThread& thread)
{
    // TArmRegSet: r0-r15, then the flags.
    uint32_t registers[32];
    TPtr8 context(reinterpret_cast<TUint8*>(registers), 0, sizeof(registers));
    thread.Context(context);
    if (context.Length() < static_cast<TInt>(16 * sizeof(uint32_t))) {
        ++samplesFailed;
        return;
    }
    uintptr_t pc = registers[15];
    uintptr_t lr = registers[14];
    auto jitStart = reinterpret_cast<uintptr_t>(JSC::startOfFixedExecutableMemoryPoolImpl());
    auto jitEnd = reinterpret_cast<uintptr_t>(JSC::endOfFixedExecutableMemoryPoolImpl());
    ++samplesBusy;
    if (pc >= imageBase && pc < imageEnd) {
        ++kindCounts[Image];
        count(((pc - imageBase) >> KBucketShift << 2) | Image);
    } else if (pc >= jitStart && pc < jitEnd) {
        ++kindCounts[Jit];
        count(((pc - jitStart) >> 12 << 2) | Jit);
    } else {
        ++kindCounts[Foreign];
        count((pc >> KBucketShift << 2) | Foreign);
        if (lr >= imageBase && lr < imageEnd) {
            ++kindCounts[ForeignCaller];
            count(((lr - imageBase) >> KBucketShift << 2) | ForeignCaller);
        }
    }
}

int compareEntries(const void* a, const void* b)
{
    uint32_t left = static_cast<const Entry*>(a)->count;
    uint32_t right = static_cast<const Entry*>(b)->count;
    return left < right ? 1 : left > right ? -1 : 0;
}

// Writes every sample so far; a later profile in the log replaces this one.
void write()
{
    samplesWritten = samplesBusy + samplesFailed;
    BrowserLog("profile: samples busy=%u idle=%u image=%u jit=%u foreign=%u foreign-caller=%u dropped=%u failed=%u buckets=%u",
        samplesBusy, samplesIdle, kindCounts[Image], kindCounts[Jit], kindCounts[Foreign], kindCounts[ForeignCaller],
        samplesDropped, samplesFailed, tableUsed);
    memcpy(sorted, table, KTableSize * sizeof(Entry));
    qsort(sorted, KTableSize, sizeof(Entry), compareEntries);
    // "kind:offset:count", most samples first; the long tail of single
    // samples is left out.
    static const char kinds[] = "ijfc";
    char line[480];
    int used = 0;
    unsigned written = 0;
    for (unsigned i = 0; i < KTableSize && sorted[i].count >= 2 && written < 6000; ++i, ++written) {
        uint32_t key = sorted[i].key;
        unsigned shift = (key & 3) == Jit ? 12 : KBucketShift;
        used += snprintf(line + used, sizeof(line) - used, " %c:%x:%u", kinds[key & 3],
            static_cast<unsigned>(key >> 2 << shift), sorted[i].count);
        if (used > 440) {
            BrowserLog("profile:%s", line);
            used = 0;
        }
    }
    if (used)
        BrowserLog("profile:%s", line);
    BrowserLog("profile: end, %u buckets written", written);
}

void* sampler(void*)
{
    RThread().SetPriority(EPriorityAbsoluteHigh);
    RThread thread;
    if (thread.Open(uiThreadId) != KErrNone) {
        stopped = true;
        return nullptr;
    }
    TTimeIntervalMicroSeconds lastCpu(0);
    bool hasCpuTime = thread.GetCpuTime(lastCpu) == KErrNone;
    while (!stopRequested && samplesBusy + samplesFailed < KMaximumSamples) {
        User::AfterHighRes(KPeriodMicroseconds);
        if (hasCpuTime) {
            // A thread that did not run since the last sample is waiting.
            TTimeIntervalMicroSeconds cpu(0);
            thread.GetCpuTime(cpu);
            bool ran = cpu.Int64() - lastCpu.Int64() >= KPeriodMicroseconds / 4;
            lastCpu = cpu;
            if (!ran) {
                ++samplesIdle;
                continue;
            }
        }
        sample(thread);
        // Also while running, as not every exit comes through
        // BrowserProfilerDump. At the UI thread's priority, as it takes a
        // while.
        if (samplesBusy + samplesFailed - samplesWritten >= KSamplesPerWrite) {
            RThread().SetPriority(EPriorityNormal);
            write();
            RThread().SetPriority(EPriorityAbsoluteHigh);
            thread.GetCpuTime(lastCpu);
        }
    }
    thread.Close();
    stopped = true;
    return nullptr;
}

} // namespace

void BrowserProfilerStart()
{
    if (table)
        return;
    TModuleMemoryInfo info;
    if (RProcess().GetMemoryInfo(info) != KErrNone)
        return;
    imageBase = info.iCodeBase;
    imageEnd = imageBase + info.iCodeSize;
    table = static_cast<Entry*>(calloc(KTableSize, sizeof(Entry)));
    sorted = static_cast<Entry*>(calloc(KTableSize, sizeof(Entry)));
    if (!table || !sorted) {
        free(table);
        free(sorted);
        table = nullptr;
        return;
    }
    uiThreadId = RThread().Id();
    BrowserLog("profile: sampling the UI thread every %d ms, image 0x%08x+0x%x",
        static_cast<int>(KPeriodMicroseconds / 1000), static_cast<unsigned>(imageBase), static_cast<unsigned>(info.iCodeSize));

    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 16 * 1024);
    pthread_t thread;
    if (pthread_create(&thread, &attributes, sampler, nullptr))
        stopped = true;
    else
        pthread_detach(thread);
    pthread_attr_destroy(&attributes);
}

void BrowserProfilerDump()
{
    if (!table)
        return;
    stopRequested = true;
    for (int waits = 0; !stopped && waits < 400; ++waits)
        User::AfterHighRes(KPeriodMicroseconds);
    if (!stopped)
        return;
    if (samplesBusy + samplesFailed != samplesWritten)
        write();
    free(table);
    free(sorted);
    table = nullptr;
}
