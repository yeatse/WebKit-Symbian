// Launch as a second guest process while the browser keeps its normal identity.
// The test runner creates the start file only after the page's unique READY log.
#include "MemoryPressureMonitorSymbianNative.h"
#include <e32base.h>
#include <stdio.h>
#include <string.h>

using namespace WTF::SymbianSystem;
int main()
{
    FILE* log = fopen("C:/Data/memory-pressure-worker.log", "w");
    if (!log)
        return 2;
    setvbuf(log, nullptr, _IONBF, 0);
    char token[34] = { };
    bool started = false;
    for (int attempt = 0; attempt < 600; ++attempt) {
        if (FILE* signal = fopen("C:/Data/memory-pressure-start", "r")) {
            size_t length = fread(token, 1, 33, signal);
            fclose(signal);
            started = length >= 32 && (length == 32 || token[32] == '\n');
            token[32] = 0;
            for (int i = 0; i < 32; ++i)
                started &= (token[i] >= '0' && token[i] <= '9') || (token[i] >= 'a' && token[i] <= 'f');
            remove("C:/Data/memory-pressure-start");
            break;
        }
        User::After(TTimeIntervalMicroSeconds32(100000));
    }
    if (!started) {
        fprintf(log, "FAIL missing/invalid unique start token\nEXIT error=%d\n", KErrTimedOut);
        fclose(log);
        return 1;
    }
    constexpr int MiB = 1024 * 1024;
    constexpr int chunkSize = 32 * MiB;
    constexpr int step = 4 * MiB;
    constexpr int hardCap = 248 * MiB;
    RChunk chunks[8];
    int sizes[8] = { };
    uint64_t before = 0, free = 0;
    bool accounting = systemFreeMemory(before) && before >= 24U * MiB;
    free = before;
    int total = 0, error = 0;
    fprintf(log, "START token=%s free=%llu hardCap=%d\n", token, before, hardCap);
    while (accounting && !error && free >= 8U * MiB && total < hardCap) {
        int index = total / chunkSize;
        if (!sizes[index])
            error = chunks[index].CreateDisconnectedLocal(0, 0, chunkSize);
        if (error)
            break;
        error = chunks[index].Commit(sizes[index], step);
        if (error)
            break;
        unsigned char* bytes = chunks[index].Base();
        for (int offset = sizes[index]; offset < sizes[index] + step; offset += 4096)
            bytes[offset] = static_cast<unsigned char>((index * 13 + offset / 4096) % 251 + 1);
        sizes[index] += step;
        total += step;
        uint64_t prior = free;
        bool valid = systemFreeMemory(free);
        // Browser allocations can reduce free RAM too. Detect the unchanged-HAL
        // baseline on the first step before accepting any threshold evidence.
        accounting = valid && (total != step || (prior >= free && prior - free >= step - 65536));
        fprintf(log, "COMMIT token=%s total=%d free=%llu error=%d accounting=%u\n", token, total, free, error, accounting);
        User::After(TTimeIntervalMicroSeconds32(20000));
    }
    bool low = accounting && !error && free < 8U * MiB;
    fprintf(log, "LOW token=%s crossed=%u free=%llu total=%d\n", token, low, free, total);
    // Bounded holding period permits the real browser observer's debounce and
    // asynchronous cache/GC work. The worker never calls into the browser or GC.
    if (low)
        User::After(TTimeIntervalMicroSeconds32(12000000));
    bool pattern = total > 0;
    for (int index = 0; index < 8; ++index) {
        const unsigned char* bytes = chunks[index].Base();
        for (int offset = 0; offset < sizes[index]; offset += 4096)
            pattern &= bytes[offset] == static_cast<unsigned char>((index * 13 + offset / 4096) % 251 + 1);
    }
    uint64_t held = 0, released = 0;
    bool heldValid = systemFreeMemory(held);
    for (auto& chunk : chunks)
        chunk.Close();
    bool releasedValid = systemFreeMemory(released);
    bool release = heldValid && releasedValid && released >= held
        && released - held + 65536 >= static_cast<uint64_t>(total);
    fprintf(log, "RELEASE token=%s bytes=%d before=%llu after=%llu pattern=%u\n", token, total, held, released, pattern);
    bool pass = accounting && low && pattern && release && !error;
    fprintf(log, "WORKER_RESULT token=%s accounting=%s low=%s pattern=%s release=%s\nEXIT error=%d\n",
        token, accounting ? "PASS" : "FAIL", low ? "PASS" : "FAIL", pattern ? "PASS" : "FAIL",
        release ? "PASS" : "FAIL", pass ? 0 : KErrGeneral);
    fclose(log);
    return pass ? 0 : 1;
}
