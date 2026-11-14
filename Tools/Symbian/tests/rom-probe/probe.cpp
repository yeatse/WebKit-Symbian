// ROM probe: names what the browser's profiler finds outside the image.
// Dumps the instructions around ROM addresses (built in, or one hexadecimal
// address per line in C:\Data\romprobe-in.txt), the code of the library
// functions the browser imports for copying, locking and allocating, and
// the time of one call of each. Runs for a few seconds and exits by itself.
// Results: C:\Data\romprobe.txt.
#include <e32base.h>
#include <e32std.h>
#include <e32atomics.h>
#include <hal.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

FILE* gLog;
void Log(const char* format, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(gLog, format, args);
    va_end(args);
    fflush(gLog);
}

const uint32_t kRomBase = 0x80000000;
const uint32_t kRomEnd = 0x90000000;

bool InRom(uint32_t address) { return address >= kRomBase && address < kRomEnd - 0x1000; }

void Dump(const char* name, uint32_t address, unsigned words)
{
    address &= ~3u;
    Log("%s %08x:", name, address);
    if (!InRom(address)) {
        Log(" not in ROM\n");
        return;
    }
    const uint32_t* code = reinterpret_cast<const uint32_t*>(address);
    for (unsigned i = 0; i < words; ++i)
        Log(" %08x", code[i]);
    Log("\n");
}

// The profiler's system addresses of build #129 on the 808, most samples first.
const uint32_t kSampled[] = {
    0x804bf510, 0x804bfd30, 0x804bf5c0, 0x804bf400, 0x804bf810, 0x804c1dc0, 0x804bf550, 0x804c2140,
    0x804c2130, 0x804ca660, 0x804ca700, 0x804c1f20, 0x804bf470, 0x804ca650, 0x804d7570, 0x804d7590,
    0x804c1d10, 0x804df8c0, 0x804df820, 0x804c1f00, 0x804c2d60, 0x804ca6f0, 0x804d6f10, 0x804debb0,
    0x804d73d0, 0x804bf200, 0x804e7b20, 0x804e7a20, 0x804df730, 0x804debd0, 0x804d73b0, 0x804bf1c0,
    0x804dec80, 0x804dec60, 0x804d7690, 0x804d73c0, 0x804bf1b0,
};

uint32_t Rotated(uint32_t instruction)
{
    uint32_t value = instruction & 0xff;
    unsigned by = ((instruction >> 8) & 0xf) * 2;
    return by ? (value >> by) | (value << (32 - by)) : value;
}

// The function an import stub of the image jumps to.
uint32_t Target(const void* function)
{
    const uint32_t* stub = reinterpret_cast<const uint32_t*>(reinterpret_cast<uintptr_t>(function) & ~3u);
    for (unsigned hop = 0; hop < 4; ++hop) {
        uint32_t address = reinterpret_cast<uint32_t>(stub);
        if ((stub[0] & 0xfffff000) == 0xe28fc000 && (stub[1] & 0xfffff000) == 0xe28cc000
            && (stub[2] & 0xfffff000) == 0xe5bcf000) {
            // add ip, pc, #a; add ip, ip, #b; ldr pc, [ip, #c]!
            uint32_t slot = address + 8 + Rotated(stub[0]) + Rotated(stub[1]) + (stub[2] & 0xfff);
            stub = reinterpret_cast<const uint32_t*>(*reinterpret_cast<const uint32_t*>(slot) & ~3u);
        } else if (stub[0] == 0xe51ff004)
            stub = reinterpret_cast<const uint32_t*>(stub[1] & ~3u);
        else if ((stub[0] & 0xff000000) == 0xea000000) {
            int32_t offset = static_cast<int32_t>(stub[0] << 8) >> 6;
            stub = reinterpret_cast<const uint32_t*>(address + 8 + offset);
        } else
            break;
    }
    return reinterpret_cast<uint32_t>(stub);
}

void Function(const char* name, const void* function, unsigned words)
{
    uint32_t target = Target(function);
    Log("function %s stub %08x target %08x\n", name, reinterpret_cast<uint32_t>(function), target);
    // Imported code is mapped wherever it is: in ROM, or in RAM for an installed library.
    const uint32_t* code = reinterpret_cast<const uint32_t*>(target);
    Log("  code:");
    for (unsigned i = 0; i < words; ++i)
        Log(" %08x", code[i]);
    Log("\n");
}

TInt gFrequency = 1;
volatile uint32_t gSink;
char gFrom[4096];
char gTo[4096];

// Function pointers the compiler cannot see through, so that each call is one.
// Set in main: the image format has no data relocation to an import.
void* (*volatile gMemcpy)(void*, const void*, size_t);
void* (*volatile gMemset)(void*, int, size_t);
void* (*volatile gMemmove)(void*, const void*, size_t);
int (*volatile gMemcmp)(const void*, const void*, size_t);
size_t (*volatile gStrlen)(const char*);

template<typename Body> void Time(const char* name, unsigned iterations, Body body)
{
    TUint32 start = User::FastCounter();
    for (unsigned i = 0; i < iterations; ++i)
        body(i);
    TUint32 ticks = User::FastCounter() - start;
    double nanoseconds = ticks * 1e9 / gFrequency / iterations;
    Log("time %-34s %8.1f ns\n", name, nanoseconds);
}

void Timings()
{
    const unsigned n = 200000;
    Log("\nfast counter %d Hz\n", gFrequency);
    Time("empty loop", n, [](unsigned i) { gSink += i; });
    static const unsigned sizes[] = { 2, 4, 8, 16, 32, 64, 256, 1024, 4096 };
    for (unsigned size : sizes) {
        char name[40];
        unsigned count = size > 256 ? n / 10 : n;
        snprintf(name, sizeof(name), "memcpy %u", size);
        Time(name, count, [size](unsigned) { gMemcpy(gTo, gFrom, size); });
        snprintf(name, sizeof(name), "memcpy %u unaligned", size);
        Time(name, count, [size](unsigned) { gMemcpy(gTo + 1, gFrom + 2, size - 1); });
        snprintf(name, sizeof(name), "memset %u", size);
        Time(name, count, [size](unsigned) { gMemset(gTo, 0, size); });
        snprintf(name, sizeof(name), "memmove %u", size);
        Time(name, count, [size](unsigned) { gMemmove(gTo + 4, gTo, size > 4 ? size - 4 : 1); });
        snprintf(name, sizeof(name), "memcmp %u equal", size);
        Time(name, count, [size](unsigned) { gSink += gMemcmp(gTo, gTo + 2048, size > 2048 ? 2048 : size); });
    }
    memset(gFrom, 'a', 63);
    Time("strlen 63", n, [](unsigned) { gSink += gStrlen(gFrom); });

    static volatile TUint32 word;
    Time("__e32_atomic_add_ord32", n, [](unsigned) { __e32_atomic_add_ord32(&word, 1); });
    Time("__e32_atomic_cas_ord32", n, [](unsigned i) { TUint32 expected = word; __e32_atomic_cas_ord32(&word, &expected, i); });
    Time("__e32_atomic_load_acq32", n, [](unsigned) { gSink += __e32_atomic_load_acq32(&word); });
    Time("compiler atomic add (ldrex)", n, [](unsigned) { __sync_fetch_and_add(&word, 1); });

    Time("User::Allocator", n, [](unsigned) { gSink += reinterpret_cast<uint32_t>(&User::Allocator()); });
    Time("User::NTickCount", n, [](unsigned) { gSink += User::NTickCount(); });
    Time("User::FastCounter", n, [](unsigned) { gSink += User::FastCounter(); });
    Time("RThread().Id()", n, [](unsigned) { gSink += RThread().Id().Id(); });
    Time("malloc 32 + free", n, [](unsigned) { void* p = malloc(32); gSink += reinterpret_cast<uint32_t>(p); free(p); });
    Time("malloc 1024 + free", n, [](unsigned) { void* p = malloc(1024); gSink += reinterpret_cast<uint32_t>(p); free(p); });
    Time("User::Alloc 32 + User::Free", n, [](unsigned) { TAny* p = User::Alloc(32); gSink += reinterpret_cast<uint32_t>(p); User::Free(p); });

    pthread_key_t key;
    pthread_key_create(&key, nullptr);
    pthread_setspecific(key, gFrom);
    Time("pthread_getspecific", n, [key](unsigned) { gSink += reinterpret_cast<uint32_t>(pthread_getspecific(key)); });
    Time("pthread_self", n, [](unsigned) { gSink += reinterpret_cast<uint32_t>(pthread_self()); });
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, nullptr);
    Time("pthread_mutex lock + unlock", n, [&mutex](unsigned) { pthread_mutex_lock(&mutex); pthread_mutex_unlock(&mutex); });

    RFastLock fastLock;
    if (fastLock.CreateLocal() == KErrNone) {
        Time("RFastLock Wait + Signal", n, [&fastLock](unsigned) { fastLock.Wait(); fastLock.Signal(); });
        fastLock.Close();
    }
    RMutex kernelMutex;
    if (kernelMutex.CreateLocal() == KErrNone) {
        Time("RMutex Wait + Signal", n, [&kernelMutex](unsigned) { kernelMutex.Wait(); kernelMutex.Signal(); });
        kernelMutex.Close();
    }
    RSemaphore semaphore;
    if (semaphore.CreateLocal(0) == KErrNone) {
        Time("RSemaphore Signal + Wait", n / 4, [&semaphore](unsigned) { semaphore.Signal(); semaphore.Wait(); });
        semaphore.Close();
    }
    RTimer timer;
    if (timer.CreateLocal() == KErrNone) {
        Time("RTimer After + Cancel", n / 20, [&timer](unsigned) {
            TRequestStatus status;
            timer.After(status, 1000000);
            timer.Cancel();
            User::WaitForRequest(status);
        });
        timer.Close();
    }
    Time("TTime::UniversalTime", n / 4, [](unsigned) { TTime now; now.UniversalTime(); gSink += static_cast<uint32_t>(now.Int64()); });
}

} // namespace

int main()
{
    gLog = fopen("c:/data/romprobe.txt", "w");
    if (!gLog)
        return 1;
    gMemcpy = memcpy;
    gMemset = memset;
    gMemmove = memmove;
    gMemcmp = memcmp;
    gStrlen = strlen;
    HAL::Get(HALData::EFastCounterFrequency, gFrequency);
    TInt machine = 0;
    HAL::Get(HALData::EMachineUid, machine);
    Log("romprobe start, machine %08x\n\nsampled addresses, from 16 bytes before each:\n", machine);

    for (uint32_t address : kSampled)
        Dump("sampled", address - 16, 12);
    if (FILE* in = fopen("c:/data/romprobe-in.txt", "r")) {
        char line[64];
        unsigned count = 0;
        while (fgets(line, sizeof(line), in) && count++ < 200)
            Dump("requested", strtoul(line, nullptr, 16) - 16, 12);
        fclose(in);
    }

    Log("\nimported functions:\n");
    Function("memcpy", reinterpret_cast<const void*>(gMemcpy), 96);
    Function("memset", reinterpret_cast<const void*>(gMemset), 64);
    Function("memmove", reinterpret_cast<const void*>(gMemmove), 64);
    Function("memcmp", reinterpret_cast<const void*>(gMemcmp), 32);
    Function("strlen", reinterpret_cast<const void*>(gStrlen), 24);
    Function("__e32_atomic_add_ord32", reinterpret_cast<const void*>(&__e32_atomic_add_ord32), 16);
    Function("__e32_atomic_cas_ord32", reinterpret_cast<const void*>(&__e32_atomic_cas_ord32), 16);
    Function("User::Allocator", reinterpret_cast<const void*>(&User::Allocator), 12);
    Function("User::NTickCount", reinterpret_cast<const void*>(&User::NTickCount), 12);
    Function("User::FastCounter", reinterpret_cast<const void*>(&User::FastCounter), 12);
    Function("User::Alloc", reinterpret_cast<const void*>(&User::Alloc), 16);
    Function("User::Free", reinterpret_cast<const void*>(&User::Free), 16);
    Function("User::WaitForAnyRequest", reinterpret_cast<const void*>(&User::WaitForAnyRequest), 8);
    Function("User::AfterHighRes", reinterpret_cast<const void*>(&User::AfterHighRes), 12);
    Function("malloc", reinterpret_cast<const void*>(&malloc), 24);
    Function("free", reinterpret_cast<const void*>(&free), 24);
    Function("pthread_getspecific", reinterpret_cast<const void*>(&pthread_getspecific), 32);
    Function("pthread_self", reinterpret_cast<const void*>(&pthread_self), 16);
    Function("pthread_mutex_lock", reinterpret_cast<const void*>(&pthread_mutex_lock), 32);

    Timings();
    Log("\ndone\n");
    fclose(gLog);
    return 0;
}
