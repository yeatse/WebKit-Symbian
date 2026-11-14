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
    ForeignCaller = 3, // Who in this executable called the code of a Foreign sample.
};

struct Entry {
    uint32_t key;
    uint32_t count;
};

const unsigned KTableBits = 16;
const unsigned KTableSize = 1 << KTableBits;
const unsigned KBucketShift = 4;
const TInt KPeriodMicroseconds = 5000;
const unsigned KMaximumSamples = 10 * 60 * 200;
const unsigned KSamplesPerWrite = 60 * 200;

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
uintptr_t stackBound;
uintptr_t stackOrigin;
TThreadId uiThreadId;
volatile bool stopRequested;
volatile bool stopped;

void count(uint32_t key)
{
    uint32_t index = (key * 2654435761u) >> (32 - KTableBits);
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

bool isInImage(uintptr_t address)
{
    return address >= imageBase && address < imageEnd;
}

// The return address into this executable nearest the top of the stack: a
// word that points behind a call instruction. System libraries call each
// other before they reach the kernel, so the link register seldom is one.
uintptr_t callerInImage(uintptr_t lr, uintptr_t sp)
{
    if (isInImage(lr))
        return lr;
    if (sp & 3 || sp < stackBound || sp >= stackOrigin)
        return 0;
    auto* words = reinterpret_cast<const uint32_t*>(sp);
    unsigned count = (stackOrigin - sp) / sizeof(uint32_t);
    if (count > 96)
        count = 96;
    for (unsigned i = 0; i < count; ++i) {
        uintptr_t word = words[i];
        if (word & 3 || !isInImage(word) || word - 4 < imageBase)
            continue;
        uint32_t instruction = *reinterpret_cast<const uint32_t*>(word - 4);
        bool isCall = (instruction & 0x0f000000) == 0x0b000000 // bl
            || (instruction & 0xfe000000) == 0xfa000000 // blx label
            || (instruction & 0x0ffffff0) == 0x012fff30; // blx register
        if (isCall)
            return word;
    }
    return 0;
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
    if (isInImage(pc)) {
        ++kindCounts[Image];
        count(((pc - imageBase) >> KBucketShift << 2) | Image);
    } else if (pc >= jitStart && pc < jitEnd) {
        ++kindCounts[Jit];
        count(((pc - jitStart) >> 12 << 2) | Jit);
    } else {
        ++kindCounts[Foreign];
        count((pc >> KBucketShift << 2) | Foreign);
        if (uintptr_t caller = callerInImage(lr, registers[13])) {
            ++kindCounts[ForeignCaller];
            count(((caller - imageBase) >> KBucketShift << 2) | ForeignCaller);
        }
    }
}

int compareEntries(const void* a, const void* b)
{
    uint32_t left = static_cast<const Entry*>(a)->count;
    uint32_t right = static_cast<const Entry*>(b)->count;
    return left < right ? 1 : left > right ? -1 : 0;
}

int compareKeys(const void* a, const void* b)
{
    uint32_t left = static_cast<const Entry*>(a)->key;
    uint32_t right = static_cast<const Entry*>(b)->key;
    return left < right ? -1 : left > right ? 1 : 0;
}

struct LineWriter {
    char line[480];
    int used { 0 };
    unsigned written { 0 };

    void add(char kind, unsigned offset, unsigned count)
    {
        used += snprintf(line + used, sizeof(line) - used, " %c:%x:%u", kind, offset, count);
        ++written;
        if (used > 440)
            flush();
    }

    void flush()
    {
        if (used)
            BrowserLog("profile:%s", line);
        used = 0;
    }
};

// Writes every sample so far; a later profile in the log replaces this one.
// "kind:offset:count", most samples first. Unless complete, the many
// addresses with one sample each are counted by 4 KB, in capitals.
void write(bool complete)
{
    samplesWritten = samplesBusy + samplesFailed;
    BrowserLog("profile: samples busy=%u idle=%u image=%u jit=%u foreign=%u foreign-caller=%u dropped=%u failed=%u buckets=%u",
        samplesBusy, samplesIdle, kindCounts[Image], kindCounts[Jit], kindCounts[Foreign], kindCounts[ForeignCaller],
        samplesDropped, samplesFailed, tableUsed);
    memcpy(sorted, table, KTableSize * sizeof(Entry));
    qsort(sorted, KTableSize, sizeof(Entry), compareEntries);
    static const char kinds[] = "ijfc";
    static const char pageKinds[] = "IJFC";
    LineWriter writer;
    unsigned i = 0;
    for (; i < KTableSize && sorted[i].count >= (complete ? 1u : 2u); ++i) {
        uint32_t key = sorted[i].key;
        unsigned shift = (key & 3) == Jit ? 12 : KBucketShift;
        writer.add(kinds[key & 3], key >> 2 << shift, sorted[i].count);
    }
    unsigned first = i;
    for (; i < KTableSize && sorted[i].count; ++i) {
        uint32_t key = sorted[i].key;
        unsigned shift = (key & 3) == Jit ? 0 : 12 - KBucketShift;
        sorted[i].key = (key >> 2 >> shift << 2) | (key & 3);
    }
    qsort(sorted + first, i - first, sizeof(Entry), compareKeys);
    for (unsigned j = first; j < i;) {
        unsigned count = 0;
        uint32_t key = sorted[j].key;
        for (; j < i && sorted[j].key == key; ++j)
            count += sorted[j].count;
        writer.add(pageKinds[key & 3], key >> 2 << 12, count);
    }
    writer.flush();
    BrowserLog("profile: end, %u counts written", writer.written);
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
            write(false);
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
    TThreadStackInfo stack;
    if (RThread().StackInfo(stack) == KErrNone) {
        stackBound = stack.iLimit;
        stackOrigin = stack.iBase;
    }
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
    write(true);
    free(table);
    free(sorted);
    table = nullptr;
}
